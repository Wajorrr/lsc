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
        struct Segment
        {
            std::unordered_map<uint64_t, Block> _items; // Segment中包含的Block列表 <lba,block>

            uint64_t _capacity;
            uint64_t _size;

            Segment(uint64_t capacity) : _capacity(capacity), _size(0) {}

            void insert(Block item)
            {
                _items.insert({item._lba, item});
                _size += item._capacity;
                assert(_size <= _capacity);
            }
        };

    public:
        BlockLog(uint64_t log_capacity, uint64_t segment_size,
                 stats::LocalStatsCollector &log_stats);

        /* ----------- Basic functionality --------------- */

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        std::vector<Block> insert(std::vector<Block> items);
        void update(std::vector<Block> items);

        /* returns true if the item is in sets layer */
        bool find(Block item);

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

    private:
        /* TODO: repetitive metadata structure, want to change in non-sim */

        void _insert(Block item);
        /* will only return forced evictions */
        std::vector<Block> _incrementSegmentAndFlush();

        stats::LocalStatsCollector &_log_stats;
        std::vector<BlockLog::Segment> _segments;
        std::unordered_map<uint64_t, int> _item_active;
        uint64_t _total_capacity;
        uint64_t _total_size;
        uint64_t _active_segment;
        uint64_t _num_segments;
        bool _track_hits_per_item;
        uint64_t _readmit;
        uint64_t _num_inserts = 0;
        uint64_t _size_inserts = 0;
    };

} // namespace flashCache
