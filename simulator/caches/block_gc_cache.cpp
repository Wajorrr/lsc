#include <cmath>

#include "block_gc_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace cache
{

    BlockGCCache::BlockGCCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : BlockCache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        /* Initialize flash cache, split size among log and
         * sets so that sets are always a multiple of setCapacity
         * and close to provided percentLog, leans toward more log */
        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存容量
        uint64_t log_capacity = flash_size_mb * 1024 * 1024;
        uint64_t cache_capacity = (uint64_t)cfg.read<int>("cache.cacheSizeMB") * 1024 * 1024;

        /* figure out what log version to use and initialize it */
        uint64_t readmit = cfg.read<int>("log.readmit", 0);
        auto &log_stats = statsCollector->createLocalCollector("log");

        std::string cache_algo_name = cfg.read<const char *>("cache.cacheAlgoName");
        auto &cache_algo_stats = statsCollector->createLocalCollector("cacheAlgo");

        if (cache_algo_name == "LRU")
        {
            INFO("Creating LRU cache\n");
            _cache_algo = new CacheAlgo::LRU(cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "FIFO")
        {
            INFO("Creating FIFO cache\n");
            _cache_algo = new CacheAlgo::FIFO(cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "S3FIFO")
        {
            INFO("Creating S3FIFO cache\n");
            _cache_algo = new CacheAlgo::S3FIFO(statsCollector, cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "SIEVE")
        {
            INFO("Creating SIEVE cache\n");
            _cache_algo = new CacheAlgo::SIEVE(cache_capacity, cache_algo_stats);
        }
        else
        {
            ERROR("Unknown cache algorithm %s\n", cache_algo_name.c_str());
            abort();
        }

        uint64_t segment_size = 1024 * 1024 * (uint64_t)cfg.read<int>("log.segmentSizeMB", 2);

        // uint64_t cache_size = log_capacity - segment_size; // 留一个段用于GC

        _log = new flashCache::BlockGC(log_capacity, segment_size, log_stats);

        /* Initialize prelog admission policy */
        if (cfg.exists("preLogAdmission")) // 是否使用准入策略
        {
            std::string policyType = cfg.read<const char *>("preLogAdmission.policy");
            std::cout << "Creating admission policy of type " << policyType << std::endl;
            policyType.append(".preLogAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preLogAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            // _prelog_admission = admission::Policy::create(admission_settings, nullptr, _log, admission_stats);
        }

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        {
            warmed_up = true;
        }
        assert(warmed_up);
    }

    BlockGCCache::~BlockGCCache()
    {
        delete _log;
        delete _cache_algo;
        // delete _prelog_admission;
    }

    void BlockGCCache::insert(const parser::Request *req)
    {
        std::vector<uint64_t> evict = _cache_algo->set(req, true);
        _log->evict(evict);
        Block id = Block::make(*req);
        if (req->type == parser::OP_SET)
            id.is_dirty = true;
        _log->insert({id});
        // INFO("insert %lu,lru size %lu, log size %lu\n", id._lba, _cache_algo->get_current_size(), _log->getTotalSize());
        if (_cache_algo->get_current_size() != _log->getTotalSize())
        {
            ERROR("lru size %lu, log size %lu\n", _cache_algo->get_current_size(), _log->getTotalSize());
            abort();
        }

        /* check warmed up condition every so often */
        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    bool BlockGCCache::find(const parser::Request *req)
    {
        // 分别查找内存缓存和flash日志缓存
        bool logic_find = _cache_algo->get(req, false);
        bool physic_find = _log->find(Block::make(*req));
        if (logic_find && physic_find)
        {
            return true;
        }
        else if (!logic_find && !physic_find)
        {
            return false;
        }
        else
        {
            ERROR("Find error, logic and physic find not match");
        }
    }

    void BlockGCCache::update(const parser::Request *req)
    {
        std::vector<uint64_t> evict = _cache_algo->set(req, true);
        _log->evict(evict);
        Block id = Block::make(*req);
        id.is_dirty = true;
        _log->update({id});
        // INFO("update %lu, lru size %lu, log size %lu\n", id._lba, _cache_algo->get_current_size(), _log->getTotalSize());
        // if (_cache_algo->get_current_size() != _log->getTotalSize())
        // {
        //     ERROR("lru size %lu, log size %lu\n", _cache_algo->get_current_size(), _log->getTotalSize());
        //     abort();
        // }
    }

    double BlockGCCache::calcFlashWriteAmp()
    { // 计算写放大
        double flash_write_amp = _log->calcWriteAmp();
        return flash_write_amp;
    }

    void BlockGCCache::checkWarmup()
    {
        // 当flash日志缓存的累计驱逐大小超过容量时，预热结束
        if (_log->ratioEvictedToCapacity() < 1)
        {
            return;
        }
        flushStats(); // 总体的统计信息
        std::cout << "Reached end of warmup, resetting stats\n\n";
        _log->flushStats(); // flash日志缓存统计信息
        dumpStats();        // 将统计信息写入输出文件
        warmed_up = true;
    }

} // namespace cache
