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

namespace cache
{

    class BlockGCCache : public virtual BlockCache
    {
    public:
        BlockGCCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~BlockGCCache();
        void insert(const parser::Request *req);
        bool find(const parser::Request *req);
        void update(const parser::Request *req);

    private:
        CacheAlgo::CacheAlgoAbstract *_cache_algo = nullptr;
    };

} // namespace cache
