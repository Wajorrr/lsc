#include <cmath>
#include <libconfig.h++>

#include "config_reader.hpp"
#include "constants.hpp"
#include "caches/set_only_cache.hpp"
#include "caches/mem_only_cache.hpp"
#include "caches/mem_log_cache.hpp"
#include "caches/mem_log_sets_cache.hpp"
#include "caches/cache.hpp"
#include "common/logging.h"

namespace cache
{

    Cache::Cache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : statsCollector(sc), globalStats(gs), _historyAccess(false)
    {
        misc::ConfigReader cfg(settings);
        int stats_power = cfg.read<int>("stats.collectionIntervalPower", STATS_INTERVAL_POWER);
        // 打印统计信息的间隔
        _stats_interval = pow(10, stats_power);
    }

    Cache::~Cache()
    {
        delete statsCollector;
    }

    Cache *Cache::create(const libconfig::Setting &settings)
    {
        misc::ConfigReader cfg(settings);

        Cache *cache_ret;

        /* initialize stats collection */
        // 打印输出到的文件路径
        std::string filename = cfg.read<const char *>("stats.outputFile");
        auto sc = new stats::StatsCollector(filename);
        // 创建一个统计信息收集器，命名为global
        auto &gs = sc->createLocalCollector("global");

        /* determine components */
        // 是否启用内存缓存
        bool memoryCache = cfg.exists("memoryCache");
        // 是否启用日志记录缓存
        bool log = cfg.exists("log");
        // 是否启用集合关联缓存
        bool sets = cfg.exists("sets");

        // 内存缓存+flash上的集合关联缓存
        if (memoryCache && !log && sets)
        {
            cache_ret = new SetOnlyCache(sc, gs, settings);
        }
        else if (memoryCache && log && sets) // 内存缓存+flash上的日志记录缓存+flash上的集合关联缓存
        {
            cache_ret = new MemLogSetsCache(sc, gs, settings);
        }
        else if (memoryCache && log && !sets) // 内存缓存+flash上的日志记录缓存
        {
            cache_ret = new MemLogCache(sc, gs, settings);
        }
        else if (memoryCache && !log && !sets) // 只有内存缓存
        {
            cache_ret = new MemOnlyCache(sc, gs, settings);
        }
        else
        {
            std::cerr << "No appropriate cache implementation for: \n"
                      << "\tMemeory Cache: " << memoryCache << "\n"
                      << "\tLog: " << log << "\n"
                      << "\tSets: " << sets
                      << std::endl;
            assert(false);
        }

        return cache_ret;
    }

    void Cache::access(const parser::Request &req)
    {
        assert(req.req_size >= 0);
        // 只统计读取请求，GET=1
        if (req.type >= parser::OP_SET)
        {
            return;
        }
        // 请求时间戳
        globalStats["timestamp"] = req.time;
        // 没有用到请求的num

        auto id = candidate_t::make(req); // 根据请求对象构造一个候选对象candidate
        bool hit = this->find(id);        // 请求是否命中

        if (hit)
        {
            globalStats["hits"]++;                  // 命中次数
            globalStats["hitsSize"] += id.obj_size; // 命中的字节数
        }
        else
        {
            globalStats["misses"]++;                  // 缺失次数
            globalStats["missesSize"] += id.obj_size; // 缺失的字节数
        }

        trackAccesses();
        trackHistory(id);

        // stats?
        if ((_stats_interval > 0) && ((getTotalAccesses() % _stats_interval) == 0))
        {
            dumpStats();
        }

        if (!hit)
        {
            this->insert(id);
        }
    }

    void Cache::dumpStats() // 打印统计信息，并输出到outputfile
    {
        INFO("Miss Rate: %lf, Flash Write Amp: %lf\n", calcMissRate(), calcFlashWriteAmp());
        // std::cout << "Miss Rate: " << calcMissRate()
        //           << " Flash Write Amp: " << calcFlashWriteAmp() << std::endl;
        statsCollector->print();
    }

    uint64_t Cache::getTotalAccesses()
    {
        return globalStats["totalAccesses"];
    }

    uint64_t Cache::getAccessesAfterFlush()
    {
        return globalStats["accessesAfterFlush"];
    }

    void Cache::trackAccesses()
    {
        globalStats["totalAccesses"]++;      // 总访问次数
        globalStats["accessesAfterFlush"]++; // 刷新后的访问次数，即一段时间窗口内的访问次数
    }

    void Cache::trackHistory(candidate_t id)
    {
        if (!_historyAccess[id]) // 若当前请求没被访问过
        {
            // first time requests are considered as compulsory misses
            globalStats["compulsoryMisses"]++; // 强制不命中次数
            _historyAccess[id] = true;
            globalStats["uniqueBytes"] += id.obj_size; // 唯一对象的总字节数，WSS
        }
    }

    double Cache::calcMissRate()
    {
        return globalStats["misses"] / (double)getAccessesAfterFlush(); // 时间窗口内的缺失率
    }

    void Cache::flushStats()
    {
        dumpStats();
        globalStats["hits"] = 0;
        globalStats["misses"] = 0;
        globalStats["hitsSize"] = 0;
        globalStats["missesSize"] = 0;
        globalStats["accessesAfterFlush"] = 0;
        globalStats["compulsoryMisses"] = 0;
        globalStats["numStatFlushes"]++;
    }

} // namespace Cache
