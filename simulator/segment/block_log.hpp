#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "block.hpp"
#include "block_log_abstract.hpp"
#include "stats/stats.hpp"

namespace flashCache
{

    // log, does lazy eviction in a round robin fashion as much as possible
    class BlockLog : public virtual BlockLogAbstract
    {

    public:
        BlockLog(uint64_t log_capacity, stats::LocalStatsCollector &log_stats);

        /* ----------- Basic functionality --------------- */

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        std::vector<Block> insert(std::vector<Block> items);

    private:
        /* TODO: repetitive metadata structure, want to change in non-sim */

        /* will only return forced evictions */
        std::vector<Block> _incrementSegmentAndFlush();

        bool _track_hits_per_item;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache
