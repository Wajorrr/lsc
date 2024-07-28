#pragma once

#include <vector>
#include "block.hpp"
#include "stats/stats.hpp"
#include "common/macro.h"

namespace flashCache
{
    struct Segment
    {
        std::unordered_map<uint64_t, Block> _items; // Segment中包含的有效块 <lba,block>

        static inline uint64_t _capacity = 0; // 段容量
        uint64_t _size;                       // 段中有效块数据量
        uint64_t _write_point;                // 已经写入了多少字节

        Segment() : _size(0), _write_point(0) {}

        void insert(Block &item)
        {
            _items.insert({item._lba, item});
            _size += item._capacity;
            _write_point += item._capacity;
            DEBUG_ASSERT(_write_point <= _capacity);
        }

        void reset()
        {
            _items.clear();
            _size = 0;
            _write_point = 0;
        }
    };

    class BlockLogAbstract
    {
    public:
        BlockLogAbstract(uint64_t log_capacity, stats::LocalStatsCollector &log_stats)
            : _current_size(0),
              _total_capacity(log_capacity),
              _active_segment(0),
              _log_stats(log_stats) {}

        /* ----------- Basic functionality --------------- */
        virtual ~BlockLogAbstract() = default;

        /* insert multiple items (allows amortization of flash write),
         * no guarantee for placement in multihash */
        virtual std::vector<Block> insert(std::vector<Block> items) = 0;
        virtual void evict(std::vector<uint64_t> items)
        {
            for (auto &id : items)
            {
                auto it = _item_active.find(id);
                if (it != _item_active.end())
                {
                    auto old_item = _segments[it->second]._items[id];
                    _log_stats["numEvictions"]++;
                    if (old_item.is_dirty)
                        _log_stats["numBlockFlushes"]++;
                    _segments[it->second]._size -= old_item._capacity;
                    // DEBUG("segments[%u]._size:%lu\n", it->second, _segments[it->second]._size);
                    _log_stats["stores_requested_bytes"] -= old_item._capacity;
                    _current_size -= old_item._capacity;
                    _segments[it->second]._items.erase(id);
                    _item_active.erase(it);
                }
            }
        }

        virtual void update(std::vector<Block> items)
        {
            // 先删除原有的记录
            for (auto item : items)
            {
                auto it = _item_active.find(item._lba);
                if (it != _item_active.end())
                {
                    auto old_item = _segments[it->second]._items[item._lba];
                    _segments[it->second]._size -= old_item._capacity;
                    // DEBUG("segments[%u]._size:%lu\n", it->second, _segments[it->second]._size);
                    _log_stats["stores_requested_bytes"] -= old_item._capacity;
                    _current_size -= old_item._capacity;
                    _segments[it->second]._items.erase(item._lba);
                    _item_active.erase(it);
                }
            }
            // 再重新插入
            insert(items);
        }

        /* returns true if the item is in sets layer */
        virtual bool find(Block item)
        {
            // 查找对象，只记录了对象是否存在，即只模拟了命中/不命中，没有真正的返回对象
            auto it = _item_active.find(item._lba);
            if (it == _item_active.end())
            {
                _log_stats["misses"]++;
                return false;
            }
            else
            {
                _log_stats["hits"]++;
                _segments[it->second]._items[item._lba].hit_count++;
                return true;
            }
        }

        int64_t get_current_size() { return _current_size; }
        int64_t get_total_size() { return _total_capacity; }

        /* ----------- Useful for Admission Policies --------------- */

        /* for readmission policies
         * shouldn't evict and only updates bytes written stats,
         * will handle "potential" evictions by remarking them as active
         * in the log even if they don't meet readmission threshold
         */
        virtual void readmit(std::vector<Block> items)
        {
            return;
        }

        /* ----------- Other Bookeeping --------------- */

        virtual double calcWriteAmp()
        {
            // 写入的字节数/实际存储的字节数=写放大
            // double ret = _log_stats["bytes_written"] / (double)_log_stats["stores_requested_bytes"];
            // 写入的字节数/用户请求写入的字节数=写放大
            double ret = _log_stats["bytes_written"] / (double)_log_stats["request_bytes_written"];
            return ret;
        }

        virtual double ratioEvictedToCapacity()
        {
            return _log_stats["sizeEvictions"] / _total_capacity;
        }

        virtual void flushStats()
        {
            _log_stats["bytes_written"] = 0;
            _log_stats["stores_requested"] = 0;
            _log_stats["stores_requested_bytes"] = 0;
            _log_stats["numEvictions"] = 0;
            _log_stats["sizeEvictions"] = 0;
            _log_stats["numLogFlushes"] = 0;
            _log_stats["misses"] = 0;
            _log_stats["hits"] = 0;
            _log_stats["num_early_evict"] = 0;
            _log_stats["size_early_evict"] = 0;
            _log_stats["bytes_rejected_from_sets"] = 0;
            _log_stats["num_rejected_from_sets"] = 0;
        }

    protected:
        void _insert(Block &item)
        {
            _log_stats["bytes_written"] += item._capacity;              // 写入字节数
            assert(_item_active.find(item._lba) == _item_active.end()); // 保证对象在当前flash Cache中不存在
            _current_size += item._capacity;
            item.hit_count = 0;
            _segments[_active_segment].insert(item); // 将对象插入当前开放的段中
            _item_active[item._lba] = _active_segment;
            assert(_segments[_active_segment]._write_point <= _segments[_active_segment]._capacity);
            // _num_inserts++;
            // _size_inserts += item.obj_size;
            // assert(_size_inserts == _segments[_active_segment]._size);
        }

    public:
        int64_t _current_size; // 存储的有效数据量
        int64_t _total_capacity;

    protected:
        std::vector<Segment> _segments;                 // 缓存段列表
        std::unordered_map<uint64_t, int> _item_active; // 标识有效块及其所在的段 <lba,segment_id>

        uint64_t _active_segment; // 开放段标号
        uint64_t _num_segments;   // 段数量

        stats::LocalStatsCollector &_log_stats;
    };

} // namespace flashCache
