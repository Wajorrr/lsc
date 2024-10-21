#pragma once
#include <libconfig.h++>
#include <unordered_map>

#include "block.hpp"
#include "block_log_abstract.hpp"
#include "parsers/parser.hpp"
#include "stats/stats.hpp"
#include "block_gc_cache.hpp"

namespace cache
{

    // Note: because candidates do not have the same size, in general:
    //
    //    accesses != hits + evictions + fills     (not equal!)
    //
    // A single access can result in multiple evictions to make space for
    // the new object. That is, evictions != misses.
    //
    // Fills is the number of misses that don't require an eviction, that
    // is there is sufficient available space to fit the object. Evictions
    // is the number of evictions, _not_ the number of misses that lead to
    // an eviction.
    class BlockRWPartitionCache : public virtual BlockCache
    {
    public:
        BlockRWPartitionCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings);
        ~BlockRWPartitionCache();

        void insert(const parser::Request *req);
        bool find(const parser::Request *req);
        void update(const parser::Request *req);

        double calcFlashWriteAmp();

        double calcCapacityUtilization();

    private:
        BlockGCCache *read_cache = nullptr;
        BlockGCCache *write_cache = nullptr;
        double read_percent;
    }; // class BlockCache

} // namespace cache
