#include <cmath>
#include <libconfig.h++>

#include "config_reader.hpp"
#include "constants.hpp"
#include "caches/block_log_cache.hpp"
#include "caches/block_gc_cache.hpp"
#include "caches/block_cache.hpp"
#include "common/logging.h"

namespace cache
{

    BlockCache::BlockCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings)
        : statsCollector(sc), globalStats(gs), _historyAccess(false), _promotFlag(false)
    {
        misc::ConfigReader cfg(settings);
        int stats_power = cfg.read<int>("stats.collectionIntervalPower", STATS_INTERVAL_POWER);

        uint64_t block_size = (uint64_t)cfg.read<int>("log.blockSize", 4096);
        Block::_capacity = block_size;

        // 打印统计信息的间隔
        _stats_interval = pow(10, stats_power);
    }

    BlockCache::~BlockCache()
    {
        delete statsCollector;
    }

    BlockCache *BlockCache::create(const libconfig::Setting &settings)
    {
        misc::ConfigReader cfg(settings);

        BlockCache *cache_ret;

        /* initialize stats collection */
        // 打印输出到的文件路径
        std::string filename = cfg.read<const char *>("stats.outputFile");
        auto sc = new stats::StatsCollector(filename);
        // 创建一个统计信息收集器，命名为global
        auto &gs = sc->createLocalCollector("global");

        bool enableGC = cfg.exists("log.enableGC");

        if (!enableGC) // 只有flash上的日志记录缓存
        {
            INFO("Creating BlockLogCache\n");
            cache_ret = new BlockLogCache(sc, gs, settings);
        }
        else if (enableGC)
        {
            INFO("Creating BlockGCCache\n");
            cache_ret = new BlockGCCache(sc, gs, settings);
        }
        else
        {
            std::cerr << "No appropriate cache implementation"
                      << std::endl;
            assert(false);
        }

        return cache_ret;
    }

    void BlockCache::access(const parser::Request &req)
    {
        assert(req.size() >= 0);
        // 统计读写请求
        if (req.type != parser::OP_GET && req.type != parser::OP_SET)
        {
            return;
        }
        // 请求时间戳
        globalStats["timestamp"] = req.time;
        // 没有用到请求的num

        auto id = Block::make(req); // 根据请求对象构造一个候选对象candidate
        bool hit = this->find(id);  // 请求是否命中

        // 需要区分两种请求：缓存负载、存储负载
        // 缓存负载请求(例如cachelib的metakv负载)中GET请求未命中时后续会进行一次SET请求
        // 存储负载请求在SET之前往往不会进行GET(否则是GET不存在的数据)
        // 一般缓存模拟默认忽略写请求或者将读写请求均视作读取请求，并默认在miss之后进行缓存写入
        // 所以这里如果采用GET miss后默认写入缓存，会导致后续原本进行缓存写入的SET请求被视作更新请求
        // 需要过滤这种负载的情况

        if (hit && req.type == parser::OP_SET)
        {
            if (_promotFlag[id])
            {
                _promotFlag[id] = false;
                return;
            }
            else
            {
                id.is_dirty = true;
                // INFO("update: %ld\n", id._lba);
                this->update(id);
                globalStats["updateCount"]++; // 更新次数
                globalStats["updateSize"] += id._capacity;
            }
        }

        if (req.type == parser::OP_GET)
        {
            if (hit)
            {
                globalStats["hits"]++;                   // 命中次数
                globalStats["hitsSize"] += id._capacity; // 命中的字节数
                if (_promotFlag[id])
                    _promotFlag[id] = false;
            }
            else
            {
                globalStats["misses"]++;                   // 缺失次数
                globalStats["missesSize"] += id._capacity; // 缺失的字节数
            }
        }

        trackAccesses(req.type);    // 统计全局和一段时间窗口内的access访问次数
        trackHistory(id, req.type); // 统计强制不命中数以及WSS

        // stats?
        if ((_stats_interval > 0) && ((getTotalAccesses() % _stats_interval) == 0))
        {
            INFO("totalAccesses: %lu, accessesAfterFlush: %lu, Printing stats\n", getTotalAccesses(), getAccessesAfterFlush());
            dumpStats();
            // flushStats();
        }

        if (!hit)
        {
            if (req.type == parser::OP_SET)
                id.is_dirty = true;
            // INFO("insert: %ld\n", id._lba);
            this->insert(id);

#ifdef CacheTrace
            if (req.type == parser::OP_GET)
                _promotFlag[id] = true;
#endif
        }
    }

    void BlockCache::dumpStats() // 打印统计信息，并输出到outputfile
    {
        INFO("Miss Rate: %lf, Flash Write Amp: %lf\n", calcMissRate(), calcFlashWriteAmp());
        // std::cout << "Miss Rate: " << calcMissRate()
        //           << " Flash Write Amp: " << calcFlashWriteAmp() << std::endl;
        statsCollector->print();
    }

    uint64_t BlockCache::getTotalAccesses()
    {
        return globalStats["totalAccesses"];
    }

    uint64_t BlockCache::getAccessesAfterFlush()
    {
        return globalStats["accessesAfterFlush"];
    }

    uint64_t BlockCache::getGetsAfterFlush()
    {
        return globalStats["GetsAfterFlush"];
    }

    void BlockCache::trackAccesses(parser::req_op_e req_op)
    {
        globalStats["totalAccesses"]++;      // 总访问次数
        globalStats["accessesAfterFlush"]++; // 刷新后的访问次数，即一段时间窗口内的访问次数
        if (req_op == parser::OP_GET)
        {
            globalStats["totalGets"]++; // 总GET请求次数
            globalStats["GetsAfterFlush"]++;
        }
        else if (req_op == parser::OP_SET)
        {
            globalStats["totalSets"]++; // 总SET请求次数
            globalStats["SetsAfterFlush"]++;
        }
    }

    void BlockCache::trackHistory(Block id, parser::req_op_e req_op)
    {
        if (!_historyAccess[id]) // 若当前请求没被访问过
        {
            // first time requests are considered as compulsory misses
            // 默认set操作为直接缓冲写入到缓存，驱逐时写入到后端
            if (req_op == parser::OP_GET)
            {
                globalStats["compulsoryMisses"]++; // 强制不命中次数
            }
            _historyAccess[id] = true;
            globalStats["uniqueBytes"] += id._capacity; // 唯一对象的总字节数，WSS
        }
    }

    double BlockCache::calcMissRate()
    {
        // return globalStats["misses"] / (double)getAccessesAfterFlush(); // 时间窗口内的缺失率
        return globalStats["misses"] / (double)getGetsAfterFlush(); // 时间窗口内的缺失率
    }

    void BlockCache::flushStats()
    {
        dumpStats();
        INFO("Flushing stats\n");
        globalStats["hits"] = 0;
        globalStats["misses"] = 0;
        globalStats["hitsSize"] = 0;
        globalStats["missesSize"] = 0;
        globalStats["accessesAfterFlush"] = 0;
        globalStats["GetsAfterFlush"] = 0;
        globalStats["SetsAfterFlush"] = 0;
        globalStats["compulsoryMisses"] = 0;
        globalStats["numStatFlushes"]++;
    }

} // namespace BlockCache