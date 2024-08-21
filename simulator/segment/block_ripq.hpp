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
    class BlockRIPQ : public virtual BlockLogAbstract
    {

    public:
        BlockRIPQ(uint64_t log_capacity, stats::LocalStatsCollector &log_stats);

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
        void _vir_incrementSegmentAndFlush(int32_t group_idx);
        std::vector<Block> group_insert(std::vector<Block> items, int32_t group_idx);
        bool find(uint64_t id, bool updateStats = false);
        void _increase(Block &block);
        void _group_insert(Block &item, int group_idx);
        void print_group();

        std::vector<Group> _group;
        // std::vector<std::list<int32_t>> _group;
        // std::vector<std::list<int32_t>::iterator> _group_active_seg;
        // std::vector<Segment> _group_virtual_seg;
        std::unordered_map<int32_t, int32_t> _group_map; // <segment_id, group_id>

        // std::unordered_map<int32_t, int32_t> _vir_group_map; // <vir_seg_id, group_id>
        std::unordered_map<int32_t, int32_t> _vir_seg_map; // <block_id, vir_seg_id>

        std::unordered_map<int32_t, int32_t> _open_vir_seg; // <group_id, vir_seg_id>

        std::list<int32_t> _free_vir_segs;

        bool _track_hits_per_item;
        const int _insertion_points = 8;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache
