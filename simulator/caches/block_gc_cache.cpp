#include <cmath>

#include "block_gc_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace cache
{

    BlockGCCache::BlockGCCache(stats::StatsCollector *sc,
                               stats::LocalStatsCollector &gs,
                               const libconfig::Setting &settings,
                               bool is_read_cache) : BlockCache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        bool enabled_rw_partition = cfg.exists("cache.enabledRWPartition");

        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存容量
        uint64_t log_capacity = flash_size_mb * 1024 * 1024;

        uint64_t cache_capacity = (uint64_t)cfg.read<int>("cache.cacheSizeMB") * 1024 * 1024;

        std::string log_name = "log";
        if (enabled_rw_partition)
        {
            double read_percent = cfg.read<double>("cache.readPercent");
            double op_percent = cfg.read<double>("cache.opPercent");

            int64_t log_size;
            if (is_read_cache)
            {
                log_size = (double)(read_percent / 100) * flash_size_mb * 1024 * 1024;
                log_size -= log_size % flashCache::Segment::_capacity;
                cache_capacity = (double)((100 - op_percent) / 100) * log_size;
                if (log_size - cache_capacity < flashCache::Segment::_capacity)
                {
                    ERROR("op percent too small\n");
                }
                log_name = "read cache";
                DEBUG("creating read cache, percent=%f, log_size=%lu, cache_capacity=%lu\n", read_percent, log_size, cache_capacity);
                misc::bytes(log_size);
            }
            else
            {
                log_size = (double)((100 - read_percent) / 100) * flash_size_mb * 1024 * 1024;
                log_size -= log_size % flashCache::Segment::_capacity;
                cache_capacity = (double)((100 - op_percent) / 100) * log_size;

                // ERROR("log_size - cache_capacity:%lu,Segment::_capacity:%lu\n", log_size - cache_capacity, flashCache::Segment::_capacity);
                if (log_size - cache_capacity < flashCache::Segment::_capacity)
                {
                    ERROR("op percent too small\n");
                }
                log_name = "write cache";
                DEBUG("creating write cache, percent=%f, log_size=%lu, cache_capacity=%lu\n", 100 - read_percent, log_size, cache_capacity);
                misc::bytes(log_size);
            }
            log_capacity = log_size;
        }
        stats::LocalStatsCollector &log_stats = statsCollector->createLocalCollector(log_name);
        _log = new flashCache::BlockGC(log_capacity, log_stats);
        if (is_read_cache)
            DEBUG("read cache log size %lu\n", log_capacity);
        else
            DEBUG("write cache log size %lu\n", log_capacity);

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
        // if (getTotalAccesses() % 1000 == 0)
        //     WARN("insert %lu, cacheAlgo size %lu, log size %lu, cache capacity %lu\n",
        //          req->id, _cache_algo->get_current_size(), _log->get_current_size(), _cache_algo->get_total_size());

        // bool test = _cache_algo->find(req, false);
        std::vector<uint64_t> evict = _cache_algo->set(req, false);
        _log->evict(evict);
        Block id = Block::make(*req);
        if (req->type == parser::OP_SET)
            id.is_dirty = true;

        _log->insert({id});

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
        bool logic_find = _cache_algo->get(req,
                                           req->type == parser::OP_GET ? true : false);
        bool physic_find = _log->find(req->id,
                                      req->type == parser::OP_GET ? true : false);
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
