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
        struct Segment
        {
            std::unordered_map<uint64_t, Block> _items; // Segment中包含的有效块 <lba,block>

            static uint64_t _capacity; // 段容量
            uint64_t _size;            // 段中有效块数据量
            uint64_t _write_point;     // 已经写入了多少字节

            Segment() : _size(0), _write_point(0) {}

            void insert(Block item)
            {
                _items.insert({item._lba, item});
                _size += item._capacity;
                _write_point += item._capacity;
                assert(_write_point <= _capacity);
            }

            void reset()
            {
                _items.clear();
                _size = 0;
                _write_point = 0;
            }
        };

    public:
        BlockGC(uint64_t log_capacity, uint64_t segment_size,
                stats::LocalStatsCollector &log_stats);

        /* ----------- Basic functionality --------------- */

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        std::vector<Block> insert(std::vector<Block> items);
        void update(std::vector<Block> items);

        /* returns true if the item is in sets layer */
        bool find(Block item);

        void evict(std::vector<uint64_t> items);

        /* ----------- Useful for Admission Policies --------------- */

        /* for readmission policies
         * shouldn't evict and only updates bytes written stats,
         * will handle "potential" evictions by remarking them as active
         * in the log even if they don't meet readmission threshold
         */
        void readmit(std::vector<Block> items);

        /* returns ratio of total capacity used */
        double ratioCapacityUsed();

        /* ----------- Other Bookeeping --------------- */

        double calcWriteAmp();
        void flushStats(); // does not print update before clearing
        double ratioEvictedToCapacity();

        uint64_t getTotalSize() { return _total_size; }

    private:
        /* TODO: repetitive metadata structure, want to change in non-sim */

        void _insert(Block item);

        uint32_t _victim_select();
        void _do_gc();
        void _incrementSegment();

        stats::LocalStatsCollector &_log_stats;
        std::vector<BlockGC::Segment> _segments; // 缓存段列表
        std::deque<uint32_t> _sealed_segments;   // 已写满的段标号列表
        std::deque<uint32_t> _free_segments;     // 空闲段标号列表

        std::unordered_map<uint64_t, int> _item_active; // 标识有效块及其所在的段 <lba,segment_id>

        // std::vector<Block> _blocks; // 用于缓存驱逐对象选取，FIFO
        // std::list<Block> _blocks;   // 用于缓存驱逐对象选取，LRU

        uint64_t _total_capacity;
        // uint64_t _cache_capacity;
        uint64_t _total_size; // 存储的有效数据量

        uint64_t _active_segment; // 开放段标号
        uint64_t _num_segments;   // 段数量

        bool _track_hits_per_item;
        uint64_t _readmit;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache
