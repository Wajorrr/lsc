#pragma once
#include <libconfig.h++>

#include "block_cache.hpp"
#include "admission/admission.hpp"
#include "segment/block_gc.hpp"
#include "cacheAlgo/lru.hpp"

namespace cache
{

    class BlockGCCache : public virtual BlockCache
    {
    public:
        BlockGCCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~BlockGCCache();
        void insert(Block id);
        bool find(Block id);
        void update(Block id);
        double calcFlashWriteAmp();
        double calcMissRate();

    private:
        void checkWarmup();

        flashCache::BlockGC *_log = nullptr;
        // admission::Policy *_prelog_admission = nullptr;
        CacheAlgo::CacheAlgoAbstract *_cache_algo = nullptr;
        bool warmed_up = false;
        bool _record_dist = false;
    };

} // namespace cache
