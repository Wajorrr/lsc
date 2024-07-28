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

        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存容量

        uint64_t log_capacity = flash_size_mb * 1024 * 1024;
        uint64_t cache_capacity = (uint64_t)cfg.read<int>("cache.cacheSizeMB") * 1024 * 1024;

        auto &log_stats = statsCollector->createLocalCollector("log");
        _log = new flashCache::BlockGC(log_capacity, log_stats);

        std::string cache_algo_name = cfg.read<const char *>("cache.cacheAlgoName");
        auto &cache_algo_stats = statsCollector->createLocalCollector("cacheAlgo");

        if (cache_algo_name == "LRU")
        {
            DEBUG("Creating LRU cache\n");
            _cache_algo = new CacheAlgo::LRU(cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "FIFO")
        {
            DEBUG("Creating FIFO cache\n");
            _cache_algo = new CacheAlgo::FIFO(cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "S3FIFO")
        {
            DEBUG("Creating S3FIFO cache\n");
            _cache_algo = new CacheAlgo::S3FIFO(statsCollector, cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "S3FIFOd")
        {
            DEBUG("Creating S3FIFOd cache\n");
            _cache_algo = new CacheAlgo::S3FIFOd(statsCollector, cache_capacity, cache_algo_stats);
        }
        else if (cache_algo_name == "SIEVE")
        {
            DEBUG("Creating SIEVE cache\n");
            _cache_algo = new CacheAlgo::SIEVE(cache_capacity, cache_algo_stats);
        }
        else
        {
            ERROR("Unknown cache algorithm %s\n", cache_algo_name.c_str());
            abort();
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
        // bool test = _cache_algo->find(req, false);
        std::vector<uint64_t> evict = _cache_algo->set(req, false);
        _log->evict(evict);
        Block id = Block::make(*req);
        if (req->type == parser::OP_SET)
            id.is_dirty = true;
        _log->insert({id});
        // INFO("insert %lu,lru size %lu, log size %lu\n", id._lba, _cache_algo->get_current_size(), _log->getTotalSize());
        if (_cache_algo->get_current_size() != _log->get_current_size())
        {
            // INFO("test %d\n", test);
            dumpStats();
            ERROR("lru size %lu, log size %lu\n", _cache_algo->get_current_size(), _log->get_current_size());
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
        std::vector<uint64_t> evict = _cache_algo->set(req, false);
        _log->evict(evict);
        Block id = Block::make(*req);
        id.is_dirty = true;
        _log->update({id});
        // INFO("update %lu, lru size %lu, log size %lu\n", id._lba, _cache_algo->get_current_size(), _log->getTotalSize());
        if (_cache_algo->get_current_size() != _log->get_current_size())
        {
            ERROR("lru size %lu, log size %lu\n", _cache_algo->get_current_size(), _log->get_current_size());
            abort();
        }
    }

} // namespace cache
