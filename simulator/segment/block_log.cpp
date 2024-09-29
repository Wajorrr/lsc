#include "constants.hpp"
#include "block_log.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    BlockLog::BlockLog(uint64_t _log_capacity, stats::LocalStatsCollector &log_stats)
        : BlockLogAbstract(_log_capacity, log_stats)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = _log_capacity / Segment::_capacity; // 包含的擦除块数量

        // Segment template_segment = Segment();
        // _segments.resize(_num_segments, template_segment); // 根据擦除块的数量和大小来分配擦除块数组空间
        init_segments();

        // printSegment();

        // allow last segment to be smaller than the others
        // 对齐到段大小，多出来的空间舍弃
        if (_log_capacity % Segment::_capacity) // 剩余空间少于一个标准段大小
        {
            _total_capacity -= _log_capacity % Segment::_capacity;
            _log_stats["logCapacity"] = _total_capacity;
            WARN("totCapacity:%ld, segment num:%d, rest capacity:%ld, align to %ld\n",
                 _log_capacity, _num_segments, _log_capacity % Segment::_capacity, _total_capacity);

            // _num_segments++;
            // // 这里_capacity是static变量，不对齐时修改了会出问题
            // template_segment._capacity = _log_capacity % Segment::_capacity; // 创建一个单独的擦除块
            // _segments.push_back(template_segment);
        }

        DEBUG("Log capacity: %ld, Num Segments: %ld, Segment Capacity: %ld\n",
              _total_capacity, _num_segments, Segment::_capacity);
        // std::cout << "Log capacity: " << _total_capacity
        //           << "\n\tNum Segments: " << _num_segments
        //           << "\n\tSegment Capacity: " << Segment::_capacity << std::endl;
    }

    std::vector<Block> BlockLog::_incrementSegmentAndFlush()
    {
        std::vector<Block> evicted;
        _active_segment = (_active_segment + 1) % _num_segments; // 新的开放块
        Segment &current_segment = *_segments[_active_segment];
        // printSegment();

        if (current_segment._size) // 如果当前擦除块中有数据
        {
            // 将当前擦除块中的有效对象加入驱逐列表
            evicted.reserve(current_segment._items.size());
            for (auto &[id, item] : current_segment._items)
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
            // _log_stats["numLogFlushes"]++;
            _log_stats["stores_requested_bytes"] -= current_segment._size;
            _current_size -= current_segment._size;
        }
        current_segment.reset(); // 重置当前擦除块
        // _num_inserts = 0;
        // _size_inserts = 0;
        return evicted;
    }

    std::vector<Block> BlockLog::insert(std::vector<Block> items)
    {
        std::vector<Block> evicted;
        for (auto &item : items)
        {
            Segment &current_segment = *_segments[_active_segment]; // 当前开放块
            // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment._size, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                /* move active segment pointer */
                std::vector<Block> local_evict = _incrementSegmentAndFlush();          // 打开一个新的开放块，可能需要驱逐一些对象
                evicted.insert(evicted.end(), local_evict.begin(), local_evict.end()); // 将驱逐的对象加入驱逐列表
            }
            _insert(item); // 将对象插入当前开放块
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["stores_requested"]++;
            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }
        assert(_total_capacity >= _current_size);
        _log_stats["current_size"] = _current_size; // 记录当前缓存对象总大小
        return evicted;
    }

} // namespace flashCache
