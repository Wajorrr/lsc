#pragma once
#include <libconfig.h++>

#include "block_cache.hpp"
#include "admission/admission.hpp"
#include "segment/block_log.hpp"
#include "segment/mblock_log.hpp"
#include "segment/block_ripq.hpp"

namespace cache
{

    class BlockLogCache : public virtual BlockCache
    {
    public:
        BlockLogCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~BlockLogCache();
        void insert(const parser::Request *req);
        bool find(const parser::Request *req);
        void update(const parser::Request *req);

    private:
    };

} // namespace cache
