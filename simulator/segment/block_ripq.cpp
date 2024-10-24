#include "block_ripq.hpp"

#include "common/logging.h"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace flashCache
{

    BlockRIPQ::BlockRIPQ(uint64_t _log_capacity, stats::LocalStatsCollector &log_stats)
        : BlockLogAbstract(_log_capacity, log_stats)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = _log_capacity / Segment::_capacity; // 包含的擦除块数量

        // Segment template_segment = Segment();
        // _segments.resize(_num_segments, template_segment); // 根据擦除块的数量和大小来分配擦除块数组空间
        init_segments();

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

        int group_num = _insertion_points;

        _group.resize(group_num);
        // _group_active_seg.resize(group_num);
        // _group_virtual_seg.resize(group_num);
        int group_size = _num_segments / group_num;
        int group_capacity = _total_capacity / group_num;
        for (int i = 0; i < group_num; i++)
        {
            for (int j = 0; j < group_size; j++)
            {
                _group[i]._segments.push_back(i * group_size + j);
                _group_map[i * group_size + j] = i;
            }
            _group[i]._active_seg = _group[i]._segments.begin();
            _group[i]._capacity = group_capacity;
        }
        int res = _num_segments % group_num;
        WARN("group_num:%d, res segment:%d\n", group_num, res);
        for (int i = _num_segments - res; i < _num_segments; i++)
        {
            _group[0]._segments.push_back(i);
            _group_map[i] = 0;
        }

        DEBUG("Log capacity: %ld, Num Segments: %ld, Segment Capacity: %ld\n",
              _total_capacity, _num_segments, Segment::_capacity);
        // DEBUG("group_num:%d\n", group_num);
        // for (int i = 0; i < group_num; i++) {
        //     DEBUG("group[%d] size:%d\n", i, _group[i]._segments.size());
        // }

        // std::cout << "Log capacity: " << _total_capacity
        //           << "\n\tNum Segments: " << _num_segments
        //           << "\n\tSegment Capacity: " << Segment::_capacity << std::endl;

        // 为ripq每个组创建一个初始开放虚拟段
        // 除了第0组
        for (int i = 1; i < group_num; i++)
        {
            // Segment template_vir_segment = Segment();
            // template_vir_segment._is_virtual = true;
            _segments.push_back(new Segment());
            _segments[_segments.size() - 1]->_is_virtual = true;
            _open_vir_seg[i] = _segments.size() - 1;
            _group_map[_segments.size() - 1] = i;
        }

        // for (int i = 0; i < _segments.size(); i++)
        // {
        //     if (_segments[i]->_is_virtual)
        //     {
        //         DEBUG("vir_seg:%d, group:%d\n", i, _group_map[i]);
        //         DEBUG("group[%d] open_vir_seg:%d\n", _group_map[i], _open_vir_seg[_group_map[i]]);
        //     }
        //     else
        //     {
        //         DEBUG("seg:%d, group:%d\n", i, _group_map[i]);
        //     }
        // }
    }

    void BlockRIPQ::check()
    {
        for (auto &p : _vir_seg_map)
        {
            if (_segments[p.second]->_items.find(p.first) == _segments[p.second]->_items.end())
            {
                ERROR("vir_seg:%d, block:%ld not find!\n", p.second, p.first);
            }
        }
    }

    void BlockRIPQ::_group_insert(Block &item, int group_idx)
    {
        int active_seg = *_group[group_idx]._active_seg;
        Segment &current_segment = *_segments[active_seg];
        _log_stats["bytes_written"] += item._capacity;              // 写入字节数
        assert(_item_active.find(item._lba) == _item_active.end()); // 保证对象在当前flash Cache中不存在
        _current_size += item._capacity;
        item.hit_count = 0;
        // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
        //       group_idx, *_group[group_idx]._active_seg, current_segment._write_point, current_segment._capacity);
        current_segment.insert(item); // 将对象插入当前开放的段中
        _item_active[item._lba] = active_seg;
        assert(current_segment._write_point <= current_segment._capacity);
        // _num_inserts++;
        // _size_inserts += item.obj_size;
        // assert(_size_inserts == current_segment._size);
    }

    void BlockRIPQ::_vir_incrementSegmentAndFlush(int32_t group_idx)
    {
        int32_t seg_idx = _open_vir_seg[group_idx];
        // DEBUG("group idx: %d,seal vir seg: %d\n", group_idx, seg_idx);
        Segment &current_segment = *_segments[seg_idx];
        if (current_segment._size == 0)
            return;
        // 将封闭虚拟段插入到当前组的开放段前的位置
        _group[group_idx]._segments.insert(_group[group_idx]._active_seg, seg_idx);
        // Segment template_vir_segment = Segment();
        // template_vir_segment._is_virtual = true;

        int32_t new_vir_seg;
        if (_free_vir_segs.size()) // 若有被驱逐或空的空闲虚拟段，直接使用
        {
            new_vir_seg = _free_vir_segs.front();
            if (_segments[new_vir_seg] == nullptr)
                _segments[new_vir_seg] = new Segment();
            _segments[new_vir_seg]->_is_virtual = true;
            _free_vir_segs.pop_front();
        }
        else // 否则创建一个新的虚拟段
        {
            _segments.push_back(new Segment());
            new_vir_seg = _segments.size() - 1;
            _segments[new_vir_seg]->_is_virtual = true;
        }

        // 将当前组的开放段指向新的虚拟段
        _open_vir_seg[group_idx] = new_vir_seg;
        _group_map[new_vir_seg] = group_idx;
    }

    std::vector<Block> BlockRIPQ::_incrementSegmentAndFlush(int32_t group_idx)
    {
        // printSegment();
        std::vector<Block> evicted;
        // DEBUG("group idx: %d, old_active_seg: %d\n", group_idx, *_group[group_idx]._active_seg);
        _group[group_idx]._active_seg++;
        // 新的开放块
        if (_group[group_idx]._active_seg == _group[group_idx]._segments.end())
            _group[group_idx]._active_seg = _group[group_idx]._segments.begin();
        // else
        //     _group[group_idx]._active_seg++;

        std::list<int32_t>::iterator temp_it = _group[group_idx]._active_seg;
        Segment *cur_seg = _segments[*temp_it];
        // DEBUG("group idx: %d, evict seg: %d\n", group_idx, *temp_it);
        // for (int i = 0; i < _segments.size(); i++)
        // {
        //     if (_segments[i]->_is_virtual)
        //     {
        //         DEBUG("vir_seg:%d, group:%d\n", i, _group_map[i]);
        //     }
        // }
        // 若为虚拟段，驱逐到低一级
        while (cur_seg->_is_virtual)
        {
            // DEBUG("group idx: %d, evict vir seg: %d\n", group_idx, *temp_it);
            _group[group_idx]._active_seg++;
            if (group_idx - 1 > 0) // 虚拟段迁移到下一个组
            {
                _group[group_idx - 1]._segments.insert(_group[group_idx - 1]._active_seg, *temp_it);
                // 更新虚拟段到组的索引
                _group_map[*temp_it] = group_idx - 1;
            }
            else // 最后一个组的虚拟段驱逐
            {
                _free_vir_segs.push_back(*temp_it); // 将虚拟段加入空闲虚拟段列表
                for (auto &item : cur_seg->_items)
                {
                    _vir_seg_map.erase(item.first); // 删除块到虚拟段的映射
                }
                _segments[*temp_it]->reset(); // 重置虚拟段
                _group_map.erase(*temp_it);   // 删除虚拟段标号到组的映射
            }
            _group[group_idx]._segments.erase(temp_it); // 从当前组的段编号列表中删除虚拟段标号

            temp_it = _group[group_idx]._active_seg;
            cur_seg = _segments[*temp_it];
        }
        DEBUG_ASSERT(_segments[*_group[group_idx]._active_seg]->_is_virtual == false);

        // 刷新物理段的同时需要刷新虚拟段，保证虚拟段的位置尽可能准确
        // 第一组没有虚拟段
        if (group_idx != 0)
            _vir_incrementSegmentAndFlush(group_idx);
        // print_group();

        // DEBUG("group idx: %d, new_active_seg: %d\n", group_idx, *_group[group_idx]._active_seg);

        // 将当前段中的数据驱逐到下一个组中，然后将当前段重置
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

    std::vector<Block> BlockRIPQ::group_insert(std::vector<Block> items, int32_t group_idx)
    {
        std::vector<Block> evicted;
        std::vector<Block> reinsert;
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
                reinsert = _incrementSegmentAndFlush(group_idx); // 打开一个新的开放块，可能需要驱逐一些对象
            }
            _group_insert(item, group_idx); // 将对象插入当前开放块

            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }

        if (group_idx == 0)
        {
            // 遍历在第1组中被驱逐的对象，若其有所属虚拟块，则重插入
            for (auto &item : reinsert)
            {
                if (_vir_seg_map.find(item._lba) == _vir_seg_map.end())
                {
                    evicted.push_back(item);
                    continue;
                }
                int vir_seg = _vir_seg_map[item._lba];
                int new_group = _group_map[vir_seg];

                // 从虚拟段中删除
                _segments[vir_seg]->_size -= item._capacity;
                _segments[vir_seg]->_items.erase(item._lba);
                _vir_seg_map.erase(item._lba);
                // 若虚拟段为空，且其不为开放虚拟段，则删除
                if (_segments[vir_seg]->_size == 0 && _open_vir_seg[new_group] != vir_seg)
                {
                    DEBUG_ASSERT(_segments[vir_seg]->_items.size() == 0);
                    _free_vir_segs.push_back(vir_seg);           // 将虚拟段加入空闲虚拟段列表
                    _group[new_group]._segments.remove(vir_seg); // 从当前组的段编号列表中删除虚拟段标号
                    _group_map.erase(vir_seg);                   // 删除虚拟段标号到组的映射
                    _segments[vir_seg]->reset();
                }

                // 重插入到新的物理段，重插入时可能会导致第1组的驱逐
                std::vector<Block> local_evict = group_insert({item}, new_group);
                evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());

                // DEBUG("reinsert item:%lu to group:%u\n", item._lba, new_group);
            }
            // 只在第1组时才会产生驱逐
            _log_stats["numEvictions"] += evicted.size();
            _log_stats["sizeEvictions"] += evited_size;
        }
        else
        {
            // 从高级别组中被驱逐的对象，降级到低级别组
            // 最后从第0组中驱逐
            std::vector<Block> local_evict = group_insert(reinsert, group_idx - 1);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
        }

        assert(_total_capacity >= _current_size);
        _log_stats["current_size"] = _current_size; // 记录当前缓存对象总大小

        return evicted;
    }

    std::vector<Block> BlockRIPQ::insert(std::vector<Block> items)
    {
        std::vector<Block> reinsert;
        std::vector<Block> evicted;
        int64_t evited_size = 0;

        for (auto &item : items)
        {
            Segment &current_segment = *_segments[*_group[0]._active_seg]; // 第一组的当前开放段
            // DEBUG("group idx: %d, current_segment: _idx:%d, _wp:%ld, _capacity:%ld\n",
            //       group_idx, *_group_active_seg[group_idx], current_segment._write_point, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                /* move active segment pointer */
                reinsert = _incrementSegmentAndFlush(0); // 打开一个新的开放块，可能需要驱逐一些对象
            }
            _group_insert(item, 0); // 将对象插入当前开放块
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["stores_requested"]++;
            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }
        // // 第一组的驱逐
        // _log_stats["numEvictions"] += evicted.size();
        // _log_stats["sizeEvictions"] += evited_size;

        // 遍历在第1组中被驱逐的对象，若其有所属虚拟块，则重插入
        for (auto &item : reinsert)
        {
            if (_vir_seg_map.find(item._lba) == _vir_seg_map.end())
            {
                evicted.push_back(item);
                continue;
            }
            int vir_seg = _vir_seg_map[item._lba];
            int new_group = _group_map[vir_seg];

            // for (int i = 0; i < _segments.size(); i++)
            // {
            //     if (_segments[i]->_is_virtual)
            //     {
            //         DEBUG("vir_seg:%d, group:%d\n", i, _group_map[i]);
            //     }
            // }
            // DEBUG("vir_seg:%d, new_group:%d\n", vir_seg, new_group);

            // 从虚拟段中删除
            _segments[vir_seg]->_size -= item._capacity;
            _segments[vir_seg]->_items.erase(item._lba);
            _vir_seg_map.erase(item._lba); // 删除块到虚拟段的映射

            // 若虚拟段为空，且其不为开放虚拟段，则删除
            if (_segments[vir_seg]->_size == 0 && _open_vir_seg[new_group] != vir_seg)
            {
                DEBUG_ASSERT(_segments[vir_seg]->_items.size() == 0);
                _free_vir_segs.push_back(vir_seg);           // 将虚拟段加入空闲虚拟段列表
                _group[new_group]._segments.remove(vir_seg); // 从当前组的段编号列表中删除虚拟段标号
                _group_map.erase(vir_seg);                   // 删除虚拟段标号到组的映射
                _segments[vir_seg]->reset();
            }

            // if (_segments[vir_seg]->_size == 0) {
            //     DEBUG("vir_seg:%d, open_vir_seg:%d\n", vir_seg, _open_vir_seg[new_group]);
            //     ERROR("vir_seg:%d, group:%d, items_num:%lu, size/4096:%lu\n",
            //           vir_seg, new_group, _segments[vir_seg]->_items.size(), _segments[vir_seg]->_size / 4096);
            // }

            // 重插入到新的物理段
            std::vector<Block> local_evict = group_insert({item}, new_group);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());

            if (_segments[vir_seg]->_items.size() != _segments[vir_seg]->_size / 4096)
            {
                ERROR("vir_seg:%d, group:%d, items_num:%lu, size/4096:%lu\n",
                      vir_seg, new_group, _segments[vir_seg]->_items.size(), _segments[vir_seg]->_size / 4096);
            }

            // print_group();
            // DEBUG("reinsert item:%lu to group:%u\n", item._lba, new_group);
        }

        assert(_total_capacity >= _current_size);
        _log_stats["current_size"] = _current_size; // 记录当前缓存对象总大小
        return evicted;
    }

    void BlockRIPQ::update(std::vector<Block> items)
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

        std::vector<Block> evicted;
        // 再重新插入
        for (auto &item : items)
        {
            // 若对象在虚拟段中，从虚拟段中删除
            if (_vir_seg_map.find(item._lba) != _vir_seg_map.end())
            {
                int vir_seg = _vir_seg_map[item._lba];
                _vir_seg_map.erase(item._lba);
                Segment &old_seg = *_segments[vir_seg];
                old_seg._size -= item._capacity;
                old_seg._items.erase(item._lba);
                // 若虚拟段为空，且其不为开放虚拟段
                if (old_seg._size == 0 && _open_vir_seg[_group_map[vir_seg]] != vir_seg)
                {
                    DEBUG_ASSERT(old_seg._items.size() == 0);
                    _free_vir_segs.push_back(vir_seg);                     // 将虚拟段加入空闲虚拟段列表
                    _group[_group_map[vir_seg]]._segments.remove(vir_seg); // 从当前组的段编号列表中删除虚拟段标号
                    _group_map.erase(vir_seg);                             // 删除虚拟段标号到组的映射
                    old_seg.reset();
                }
            }
            std::vector<Block> local_evict = group_insert({item}, 0);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
        }

        // return evicted;

        // if (group_idx == 0)
        //     group_insert(items, group_idx);
        // else
        //     group_insert(items, group_idx - 1);
    }

    void BlockRIPQ::_increase(Block &block)
    {
        int id = block._lba;
        auto it = _vir_seg_map.find(id);

        int old_group, new_group, open_vir_seg;

        // 若对象在虚拟段中，更新到新的虚拟段
        if (it != _vir_seg_map.end())
        {
            old_group = _group_map[it->second];
            // DEBUG("old group:%d\n", old_group);
            // DEBUG("vir_seg:%d, group:%d\n", it->second, _group_map[it->second]);
            // DEBUG("group[%d] open_vir_seg:%d\n\n", _group_map[it->second], _open_vir_seg[_group_map[it->second]]);
            new_group = old_group == _group.size() - 1 ? old_group : old_group + 1;
            open_vir_seg = _open_vir_seg[new_group];
            // DEBUG("new group:%d, open_vir_seg:%d\n", new_group, open_vir_seg);
            // 若虚拟段已满，刷新虚拟段
            if (_segments[open_vir_seg]->_size + Block::_capacity > _segments[open_vir_seg]->_capacity)
            {
                // DEBUG("sealed vir seg:%d\n", open_vir_seg);
                _vir_incrementSegmentAndFlush(new_group);
                open_vir_seg = _open_vir_seg[new_group];
            }
            Segment &_old_segment = *_segments[_vir_seg_map[id]];
            int old_seg_idx = it->second;

            // WARN("vir_seg:%d, group:%d\n", it->second, _group_map[it->second]);

            // WARN("vir_seg:%d, group:%d\n", it->second, _group_map[it->second]);

            // 从原虚拟段中删除
            _old_segment._size -= Block::_capacity;
            _old_segment._items.erase(id);
            if (_old_segment._items.size() != _old_segment._size / 4096)
            {
                DEBUG("block id:%d, old seg idx:%d, new seg idx:%d\n", id, old_seg_idx, open_vir_seg);
                ERROR("vir_seg:%d, group:%d, items_num:%lu, size/4096:%lu\n",
                      open_vir_seg, new_group, _old_segment._items.size(), _old_segment._size / 4096);
            }
            // 插入到新虚拟段
            _segments[open_vir_seg]->_items.insert({id, block});
            _segments[open_vir_seg]->_size += Block::_capacity;
            // 更新到新虚拟段的索引
            _vir_seg_map[id] = open_vir_seg;

            // 若原虚拟段为空，且其不为开放虚拟段
            if (_old_segment._size == 0 && _open_vir_seg[old_group] != old_seg_idx)
            {
                old_group = _group_map[old_seg_idx];
                // print_group();
                // WARN("erase old vir seg:%d\n, group %d's open_vir_seg:%d\n", old_seg_idx, old_group, _open_vir_seg[old_group]);
                DEBUG_ASSERT(_old_segment._items.size() == 0);
                _free_vir_segs.push_back(old_seg_idx);
                _group[old_group]._segments.remove(old_seg_idx);
                _group_map.erase(old_seg_idx);
                _old_segment.reset();
            }
        }
        else // 对象不在虚拟段中，则更新到物理段所在组的高一级组
        {
            old_group = _group_map[_item_active[id]];
            new_group = old_group == _group.size() - 1 ? old_group : old_group + 1;
            open_vir_seg = _open_vir_seg[new_group];
            if (_segments[open_vir_seg]->_items.size() != _segments[open_vir_seg]->_size / 4096)
            {
                ERROR("vir_seg:%d, group:%d, items_num:%lu, size/4096:%lu\n",
                      open_vir_seg, new_group, _segments[open_vir_seg]->_items.size(), _segments[open_vir_seg]->_size / 4096);
            }
            // 若虚拟段已满，刷新虚拟段
            if (_segments[open_vir_seg]->_size + Block::_capacity > _segments[open_vir_seg]->_capacity)
            {
                // DEBUG("sealed vir seg:%d\n", open_vir_seg);
                _vir_incrementSegmentAndFlush(new_group);
                open_vir_seg = _open_vir_seg[new_group];
            }

            // 插入到新虚拟段
            _segments[open_vir_seg]->_items.insert({id, block});
            _segments[open_vir_seg]->_size += Block::_capacity;
            // 更新到新虚拟段的索引
            _vir_seg_map[id] = open_vir_seg;
        }
        // DEBUG("increase item:%lu from group:%d to group:%d\n", id, old_group, new_group);
        // print_group();
        // for (int i = 0; i < _segments.size(); i++)
        // {
        //     if (_segments[i]->_is_virtual)
        //     {
        //         DEBUG("vir_seg:%d, group:%d\n", i, _group_map[i]);
        //         DEBUG("group[%d] open_vir_seg:%d\n\n", _group_map[i], _open_vir_seg[_group_map[i]]);
        //     }
        // }
        // if (_group_map[104] != 1)
        // {
        //     ERROR("104 in group:%d\n", _group_map[104]);
        // }
    }

    bool BlockRIPQ::find(uint64_t id, bool updateStats)
    {
        // 查找对象，只记录了对象是否存在，即只模拟了命中/不命中，没有真正的返回对象
        auto it = _item_active.find(id);
        if (it == _item_active.end())
        {
            if (updateStats)
                _log_stats["misses"]++;
            return false;
        }
        else
        {
            if (updateStats)
            {
                _log_stats["hits"]++;
                _segments[it->second]->_items[id].hit_count++;
                _increase(_segments[it->second]->_items[id]);
            }
            return true;
        }
    }

    void BlockRIPQ::print_group()
    {
        for (int i = 0; i < _group.size(); i++)
        {
            DEBUG("group[%d]:\n", i);
            for (auto &seg : _group[i]._segments)
            {
                if (_segments[seg]->_is_virtual)
                    printf("vir seg:%d -> ", seg);
                else
                    printf("phy seg:%d -> ", seg);
            }
            printf("\n");
        }
    }

} // namespace flashCache
