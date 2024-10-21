#pragma once
#include <libconfig.h++>

#include "block_cache.hpp"
#include "admission/admission.hpp"
#include "segment/block_gc.hpp"
#include "cacheAlgo/lru.hpp"
#include "cacheAlgo/fifo.hpp"
#include "cacheAlgo/s3fifo.hpp"
#include "cacheAlgo/s3fifod.hpp"
#include "cacheAlgo/sieve.hpp"
#include "common/bytes.hpp"

namespace cache
{

    class BlockGCCache : public virtual BlockCache
    {
    public:
        BlockGCCache(stats::StatsCollector *sc,
                     stats::LocalStatsCollector &gs,
                     const libconfig::Setting &settings,
                     bool is_read_cache = false);
        ~BlockGCCache();
        void insert(const parser::Request *req);
        bool find(const parser::Request *req);
        void update(const parser::Request *req);
        void print_config()
        {
            INFO("BlockGCCache\n");
            INFO("log size %lu, current size %lu, segment num %lu\n",
                 _log->get_total_size(), _log->get_current_size(), _log->get_segments_num());
            INFO("cache size %lu, current size %lu\n", _cache_algo->get_total_size(), _cache_algo->get_current_size());
        }
        double calcCapacityUtilization()
        {
            // DEBUG("current_size:%lu, total_size:%lu\n", _cache_algo->get_current_size(), _log->get_total_size());
            double utilization = _cache_algo->get_current_size() / (double)_log->get_total_size();
            return utilization;
        }

    private:
        CacheAlgo::CacheAlgoAbstract *_cache_algo = nullptr;
    };

} // namespace cache
