#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "block.hpp"
#include "block_log_abstract.hpp"
#include "stats/stats.hpp"
#include "deque"

namespace flashCache
{

    // log, does lazy eviction in a round robin fashion as much as possible
    class BlockGC : public virtual BlockLogAbstract
    {

    public:
        BlockGC(uint64_t _log_capacity, stats::LocalStatsCollector &_log_stats);

        /* ----------- Basic functionality --------------- */

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        std::vector<Block> insert(std::vector<Block> items);

    private:
        /* TODO: repetitive metadata structure, want to change in non-sim */
        uint32_t _victim_select();
        void _do_gc();
        void _incrementSegment();

        std::deque<uint32_t> _sealed_segments; // 已写满的段标号列表
        std::deque<uint32_t> _free_segments;   // 空闲段标号列表

        // std::vector<Block> _blocks; // 用于缓存驱逐对象选取，FIFO
        // std::list<Block> _blocks;   // 用于缓存驱逐对象选取，LRU

        // uint64_t _cache_capacity;

        bool _track_hits_per_item;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache