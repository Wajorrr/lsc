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
    class mBlockLog : public virtual BlockLogAbstract
    {

    public:
        mBlockLog(uint64_t log_capacity, stats::LocalStatsCollector &log_stats);

        /* ----------- Basic functionality --------------- */

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        std::vector<Block> insert(std::vector<Block> items);
        void update(std::vector<Block> items);

        // int64_t get_current_size();
        // int64_t get_total_size();

    private:
        /* TODO: repetitive metadata structure, want to change in non-sim */

        /* will only return forced evictions */
        std::vector<Block> _incrementSegmentAndFlush(int32_t group_idx);
        std::vector<Block> group_insert(std::vector<Block> &items, int32_t group_idx);
        void _group_insert(Block &item, int group_idx);
        void print_group();

        std::vector<Group> _group;
        // std::vector<std::list<int32_t>> _group;
        // std::vector<std::list<int32_t>::iterator> _group_active_seg;
        std::unordered_map<int32_t, int32_t> _group_map; // <segment_id, group_id>

        bool _track_hits_per_item;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache
