#pragma once
#include <libconfig.h++>

#include "admission/admission.hpp"
#include "caches/write_cache.hpp"
#include "segment/fifo_log.hpp"
#include "segment/block_log.hpp"

namespace cache
{

    class LogOnlyCache : public virtual WriteCache
    {
    public:
        LogOnlyCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~LogOnlyCache();
        void insert(candidate_t id);
        bool find(candidate_t id);
        void update(candidate_t id);
        double calcFlashWriteAmp();
        double calcMissRate();

    private:
        void checkWarmup();

        flashCache::FIFOLog *_log = nullptr;
        // admission::Policy *_prelog_admission = nullptr;
        bool warmed_up = false;
        bool _record_dist = false;
    };

} // namespace cache
