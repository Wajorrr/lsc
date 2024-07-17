#include "constants.hpp"
#include "block_log.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    BlockLog::BlockLog(uint64_t log_capacity, uint64_t segment_size,
                       stats::LocalStatsCollector &log_stats) : _log_stats(log_stats),
                                                                _total_capacity(log_capacity),
                                                                _total_size(0),
                                                                _active_segment(0),
                                                                _readmit(0)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = log_capacity / segment_size; // 包含的擦除块数量

        Segment template_segment = Segment(segment_size);
        _segments.resize(_num_segments, template_segment); // 根据擦除块的数量和大小来分配擦除块数组空间
        // allow last segment to be smaller than the others
        if (log_capacity % segment_size) // 剩余空间少于一个标准擦除块大小
        {
            _num_segments++;
            template_segment._capacity = log_capacity % segment_size; // 创建一个单独的擦除块
            _segments.push_back(template_segment);
        }
        std::cout << "Log capacity: " << _total_capacity
                  << "\n\tNum Segments: " << _num_segments
                  << "\n\tSegment Capacity: " << segment_size << std::endl;
    }

    void BlockLog::_insert(Block item)
    {
        _log_stats["bytes_written"] += item._capacity; // 写入字节数
        _log_stats["request_bytes_written"] += item._capacity;
        _log_stats["stores_requested"]++;
        _log_stats["stores_requested_bytes"] += item._capacity;     // 存储字节数
        assert(_item_active.find(item._lba) == _item_active.end()); // 保证对象在当前flash Cache中不存在
        _total_size += item._capacity;
        item.hit_count = 0;
        _segments[_active_segment].insert(item); // 将对象插入当前开放的擦除块中
        _item_active[item._lba] = _active_segment;
        assert(_segments[_active_segment]._size <= _segments[_active_segment]._capacity);
        // _num_inserts++;
        // _size_inserts += item.obj_size;
        // assert(_size_inserts == _segments[_active_segment]._size);
    }

    std::vector<Block> BlockLog::_incrementSegmentAndFlush()
    {
        std::vector<Block> evicted;
        _active_segment = (_active_segment + 1) % _num_segments; // 新的开放块
        Segment &current_segment = _segments[_active_segment];

        if (current_segment._size) // 如果当前擦除块中有数据
        {
            // 将当前擦除块中的有效对象加入驱逐列表
            evicted.reserve(current_segment._items.size());
            for (auto [id, item] : current_segment._items)
            {
                if (_item_active.find(item._lba) != _item_active.end())
                {
                    // only move if not already in sets
                    evicted.push_back(item);
                }
                // should always remove an item, otherwise code bug
                _item_active.erase(item._lba);
            }
            _log_stats["numEvictions"] += current_segment._items.size();
            _log_stats["sizeEvictions"] += current_segment._size;
            _log_stats["numLogFlushes"]++;
            _log_stats["stores_requested_bytes"] -= current_segment._size;
            _total_size -= current_segment._size;
            current_segment._items.clear();
            current_segment._size = 0;
        }
        // _num_inserts = 0;
        // _size_inserts = 0;
        return evicted;
    }

    std::vector<Block> BlockLog::insert(std::vector<Block> items)
    {
        std::vector<Block> evicted;
        for (auto item : items)
        {
            Segment &current_segment = _segments[_active_segment]; // 当前开放块
            // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment._size, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._size > current_segment._capacity) // 当前开放块空间不够了
            {
                /* move active segment pointer */
                std::vector<Block> local_evict = _incrementSegmentAndFlush();          // 打开一个新的开放块，可能需要驱逐一些对象
                evicted.insert(evicted.end(), local_evict.begin(), local_evict.end()); // 将驱逐的对象加入驱逐列表
            }
            _insert(item); // 将对象插入当前开放块
        }
        assert(_total_capacity >= _total_size);
        _log_stats["current_size"] = _total_size; // 记录当前缓存对象总大小
        return evicted;
    }

    void BlockLog::update(std::vector<Block> items)
    {
        for (auto item : items)
        {
            // _log_stats["stores_requested"]++;
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["bytes_written"] += item._capacity;
            auto it = _item_active.find(item._lba);
            if (it != _item_active.end())
            {
                auto old_item = _segments[it->second]._items[item._lba];
                _segments[it->second]._size -= old_item._capacity;
                _log_stats["stores_requested_bytes"] -= old_item._capacity;
                _total_size -= old_item._capacity;
                _segments[it->second]._items.erase(item._lba);
                _item_active.erase(it);
            }
        }
        insert(items);
    }

    void BlockLog::readmit(std::vector<Block> items)
    {
        return;
    }

    // 查找对象，只记录了对象是否存在，即只模拟了命中/不命中，没有真正的返回对象
    bool BlockLog::find(Block item)
    {
        auto it = _item_active.find(item._lba);
        if (it == _item_active.end())
        {
            _log_stats["misses"]++;
            return false;
        }
        else
        {
            _log_stats["hits"]++;
            return true;
        }
    }

    double BlockLog::ratioCapacityUsed()
    {
        return _total_size / _total_capacity;
    }

    double BlockLog::calcWriteAmp()
    { // 写入的字节数/实际存储的字节数=写放大
        // double ret = _log_stats["bytes_written"] / (double)_log_stats["stores_requested_bytes"];
        // 写入的字节数/用户请求写入的字节数=写放大
        double ret = _log_stats["bytes_written"] / (double)_log_stats["request_bytes_written"];
        return ret;
    }

    void BlockLog::flushStats()
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

    double BlockLog::ratioEvictedToCapacity()
    {
        return _log_stats["sizeEvictions"] / _total_capacity;
    }

} // namespace flashCache
