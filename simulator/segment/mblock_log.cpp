#include "constants.hpp"
#include "mblock_log.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    mBlockLog::mBlockLog(uint64_t _log_capacity, stats::LocalStatsCollector &log_stats)
        : BlockLogAbstract(_log_capacity, log_stats)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = _log_capacity / Segment::_capacity; // 包含的擦除块数量

        // Segment template_segment = Segment();
        // _segments.resize(_num_segments, template_segment); // 根据擦除块的数量和大小来分配擦除块数组空间
        _segments.resize(_num_segments, new Segment()); // 根据擦除块的数量和大小来分配擦除块数组空间

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

        int group_num = 3;
        _group.resize(group_num);
        int group_size = _num_segments / group_num;
        for (int i = 0; i < group_num; i++)
        {
            for (int j = 0; j < group_size; j++)
            {
                _group[i]._segments.push_back(i * group_size + j);
                _group_map[i * group_size + j] = i;
            }
            _group[i]._active_seg = _group[i]._segments.begin();
        }
        int res = _num_segments % group_num;
        for (int i = _num_segments - res; i < _num_segments; i++)
        {
            _group[0]._segments.push_back(i);
            _group_map[i] = 0;
        }

        DEBUG("Log capacity: %ld, Num Segments: %ld, Segment Capacity: %ld\n",
              _total_capacity, _num_segments, Segment::_capacity);
        DEBUG("group_num:%d\n", group_num);
        for (int i = 0; i < group_num; i++)
        {
            DEBUG("group[%d] size:%d\n", i, _group[i]._segments.size());
        }
        // std::cout << "Log capacity: " << _total_capacity
        //           << "\n\tNum Segments: " << _num_segments
        //           << "\n\tSegment Capacity: " << Segment::_capacity << std::endl;
    }

    void mBlockLog::_group_insert(Block &item, int group_idx)
    {
        int active_seg = *_group[group_idx]._active_seg;
        Segment &current_segment = *_segments[active_seg];
        _log_stats["bytes_written"] += item._capacity;              // 写入字节数
        assert(_item_active.find(item._lba) == _item_active.end()); // 保证对象在当前flash Cache中不存在
        _current_size += item._capacity;
        item.hit_count = 0;
        // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
        //       group_idx, *_group_active_seg[group_idx], current_segment._write_point, current_segment._capacity);
        current_segment.insert(item); // 将对象插入当前开放的段中
        _item_active[item._lba] = active_seg;
        assert(current_segment._write_point <= current_segment._capacity);
        // _num_inserts++;
        // _size_inserts += item.obj_size;
        // assert(_size_inserts == current_segment._size);
    }

    std::vector<Block> mBlockLog::_incrementSegmentAndFlush(int32_t group_idx)
    {
        std::vector<Block> evicted;
        // 新的开放块
        if (_group[group_idx]._active_seg == _group[group_idx]._segments.end())
            _group[group_idx]._active_seg = _group[group_idx]._segments.begin();
        else
            _group[group_idx]._active_seg++;
        // _active_segment = (_active_segment + 1) % _num_segments;
        Segment &current_segment = *_segments[*_group[group_idx]._active_seg];

        if (current_segment._size) // 如果当前擦除块中有数据
        {
            // 将当前擦除块中的有效对象加入驱逐列表
            evicted.reserve(current_segment._items.size());
            for (auto &[id, item] : current_segment._items)
            {
                if (_item_active.find(item._lba) != _item_active.end())
                {
                    evicted.push_back(item);
                }
                // should always remove an item, otherwise code bug
                _item_active.erase(item._lba);
            }
            // _log_stats["numEvictions"] += current_segment._items.size();
            // _log_stats["sizeEvictions"] += current_segment._size;

            _log_stats["stores_requested_bytes"] -= current_segment._size;
            _current_size -= current_segment._size;

            // _log_stats["numLogFlushes"]++;
        }
        current_segment.reset(); // 重置当前擦除块

        return evicted;
    }
    std::vector<Block> mBlockLog::group_insert(std::vector<Block> &items, int32_t group_idx)
    {
        std::vector<Block> reinsert;
        std::vector<Block> evicted;
        int64_t evited_size = 0;

        for (auto &item : items)
        {
            Segment &current_segment = *_segments[*_group[group_idx]._active_seg]; // 第一组的当前开放块
            // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
            //       group_idx, *_group_active_seg[group_idx], current_segment._write_point, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                /* move active segment pointer */
                std::vector<Block> local_evict = _incrementSegmentAndFlush(group_idx); // 打开一个新的开放块，可能需要驱逐一些对象
                for (auto &item : local_evict)
                {
                    if (item.hit_count > 0)
                    {
                        reinsert.push_back(item);
                    }
                    else
                    {
                        evicted.push_back(item);
                        evited_size += item._capacity;
                    }
                }
            }
            _group_insert(item, group_idx); // 将对象插入当前开放块

            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }
        // 只统计当前组的驱逐
        _log_stats["numEvictions"] += evicted.size();
        _log_stats["sizeEvictions"] += evited_size;

        // 重新插入
        if (reinsert.size())
        {
            std::vector<Block> group_evicted;
            if (group_idx == _group.size() - 1)
            {
                // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
                //       group_idx, *_group_active_seg[group_idx], current_segment._write_point, current_segment._capacity);
                // print_group();
                group_evicted = group_insert(reinsert, group_idx);
            }
            else
            {
                group_evicted = group_insert(reinsert, group_idx + 1);
            }
            // 回溯时将每次驱逐的对象合并
            evicted.insert(evicted.end(), group_evicted.begin(), group_evicted.end());
        }

        assert(_total_capacity >= _current_size);
        _log_stats["current_size"] = _current_size; // 记录当前缓存对象总大小
        return evicted;
    }

    std::vector<Block> mBlockLog::insert(std::vector<Block> items)
    {
        std::vector<Block> reinsert;
        std::vector<Block> evicted;
        int64_t evited_size = 0;

        for (auto &item : items)
        {
            Segment &current_segment = *_segments[*_group[0]._active_seg]; // 第一组的当前开放块
            // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
            //       group_idx, *_group_active_seg[group_idx], current_segment._write_point, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                /* move active segment pointer */
                std::vector<Block> local_evict = _incrementSegmentAndFlush(0); // 打开一个新的开放块，可能需要驱逐一些对象
                for (auto &item : local_evict)
                {
                    if (item.hit_count > 0)
                    {
                        reinsert.push_back(item);
                    }
                    else
                    {
                        evicted.push_back(item);
                        evited_size += item._capacity;
                    }
                }
            }
            _group_insert(item, 0); // 将对象插入当前开放块
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["stores_requested"]++;
            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }
        // 只统计当前组的驱逐
        _log_stats["numEvictions"] += evicted.size();
        _log_stats["sizeEvictions"] += evited_size;

        // 重新插入
        if (reinsert.size())
        {
            std::vector<Block> group_evicted = group_insert(reinsert, 1);
            // 回溯时将每次驱逐的对象合并
            evicted.insert(evicted.end(), group_evicted.begin(), group_evicted.end());
        }

        assert(_total_capacity >= _current_size);
        _log_stats["current_size"] = _current_size; // 记录当前缓存对象总大小
        return evicted;
    }

    void mBlockLog::update(std::vector<Block> items)
    {
        int group_idx = 0;
        // 先删除原有的记录
        for (auto &item : items)
        {
            auto it = _item_active.find(item._lba);
            if (it != _item_active.end())
            {
                group_idx = _group_map[it->second];
                auto old_item = _segments[it->second]->_items[item._lba];
                _segments[it->second]->_size -= old_item._capacity;
                // DEBUG("segments[%u]._size:%lu\n", it->second, _segments[it->second]->_size);
                _log_stats["stores_requested_bytes"] -= old_item._capacity;
                _current_size -= old_item._capacity;
                _segments[it->second]->_items.erase(item._lba);
                _item_active.erase(it);
            }
            // 由于更新造成的每个对象被重新插入，计入请求写入字节数
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["stores_requested"]++;
        }
        // 再重新插入
        if (group_idx == 0)
            group_insert(items, group_idx);
        else
            group_insert(items, group_idx - 1);
    }

    void mBlockLog::print_group()
    {
        for (int i = 0; i < _group.size(); i++)
        {
            DEBUG("group[%d]:\n", i);
            for (auto &seg : _group[i]._segments)
            {
                printf("seg:%d -> ", seg);
            }
            printf("\n");
        }
    }

} // namespace flashCache
