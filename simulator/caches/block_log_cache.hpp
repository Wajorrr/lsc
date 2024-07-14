#pragma once
#include <libconfig.h++>

#include "block_cache.hpp"
#include "admission/admission.hpp"
#include "segment/block_log.hpp"

namespace cache
{

    class BlockLogCache : public virtual BlockCache
    {
    public:
        BlockLogCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~BlockLogCache();
        void insert(Block id);
        bool find(Block id);
        void update(Block id);
        double calcFlashWriteAmp();
        double calcMissRate();

    private:
        void checkWarmup();

        flashCache::BlockLog *_log = nullptr;
        // admission::Policy *_prelog_admission = nullptr;
        bool warmed_up = false;
        bool _record_dist = false;
    };

} // namespace cache
