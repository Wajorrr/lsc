#include "constants.hpp"
#include "block_gc.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    uint64_t BlockGC::Segment::_capacity = 0; // 或其他默认值

    BlockGC::BlockGC(uint64_t log_capacity, uint64_t segment_size,
                     stats::LocalStatsCollector &log_stats) : _log_stats(log_stats),
                                                              _total_capacity(log_capacity),
                                                              _total_size(0),
                                                              _active_segment(0),
                                                              _readmit(0)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = log_capacity / segment_size; // 包含的段数量

        Segment::_capacity = segment_size;
        Segment template_segment = Segment();
        _segments.resize(_num_segments, template_segment); // 根据段的数量和大小来分配段数组空间
        for (auto &segment : _segments)
        {
            segment.reset();
        }

        // allow last segment to be smaller than the others
        if (log_capacity % segment_size) // 剩余空间少于一个标准段大小
        {
            _num_segments++;
            template_segment._capacity = log_capacity % segment_size; // 创建一个单独的段
            _segments.push_back(template_segment);
        }

        // 初始化_sealed_segments为空
        _sealed_segments.clear();

        // 初始化_free_segments为1到_num_segments-1(0为默认初始开放段)
        for (uint64_t i = 1; i < _num_segments; ++i)
        {
            _free_segments.push_back(i);
        }

        std::cout << "Log capacity: " << _total_capacity
                  << "\n\tNum Segments: " << _num_segments
                  << "\n\tSegment Capacity: " << segment_size << std::endl;
    }

    void BlockGC::_insert(Block item)
    {
        _log_stats["bytes_written"] += item._capacity;              // 写入字节数
        assert(_item_active.find(item._lba) == _item_active.end()); // 保证对象在当前flash Cache中不存在
        _total_size += item._capacity;
        // item.hit_count = 0;
        _segments[_active_segment].insert(item); // 将对象插入当前开放的段中
        _item_active[item._lba] = _active_segment;
        assert(_segments[_active_segment]._write_point <= _segments[_active_segment]._capacity);
        // _num_inserts++;
        // _size_inserts += item.obj_size;
        // assert(_size_inserts == _segments[_active_segment]._size);
    }

    uint32_t BlockGC::_victim_select()
    {
        auto min_it = std::min_element(_sealed_segments.begin(), _sealed_segments.end(),
                                       [this](const uint32_t &a, const uint32_t &b)
                                       {
                                           return _segments[a]._size < _segments[b]._size;
                                       });
        uint32_t min_idx = *min_it;
        _sealed_segments.erase(min_it);
        return min_idx;
    }

    void BlockGC::_do_gc()
    {
        uint64_t total_reclaimed = 0;
        std::vector<Block> rewrite_blocks;
        // 选取若干个段GC，保证GC回收的空间不小于一个段的大小
        while (total_reclaimed < Segment::_capacity)
        {
            uint32_t victim_idx = _victim_select(); // 选择一个段进行GC
            // DEBUG("victim_idx:%u\n", victim_idx);
            Segment &victim = _segments[victim_idx];
            uint64_t total_rewrite = 0;
            for (auto &item : victim._items)
            {
                rewrite_blocks.push_back(item.second);
                _item_active.erase(item.first);
                total_rewrite += item.second._capacity;
            }
            _total_size -= victim._size;
            // DEBUG("total_reclaimed:%lu, victim._size:%lu\n", total_rewrite, victim._size);
            total_reclaimed += Segment::_capacity - victim._size;
            victim.reset();
            _free_segments.push_back(victim_idx); // 将被GC的段加入空闲段列表
        }
        // 有效数据迁移
        for (auto item : rewrite_blocks)
        {
            Segment &current_segment = _segments[_active_segment]; // 当前开放块
            // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment._size, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                _incrementSegment(); // 当前开放块已写满，切换到下一个开放块
            }
            _insert(item); // 将对象插入当前开放块
        }
    }

    // void debug_segments()
    // {
    //     uint64_t temptotSize = 0;
    //     for (auto segment_idx : _sealed_segments)
    //     {
    //         Segment &segment = _segments[segment_idx];
    //         temptotSize += segment._size;
    //         DEBUG("segments[%u]._size:%lu, segment._capacity:%lu\n", segment_idx, segment._size, segment._capacity);
    //         if (segment._size > segment._capacity)
    //         {
    //             abort();
    //         }
    //     }
    //     DEBUG("temptotSize:%lu, _total_size:%lu\n", temptotSize, _total_size);
    // }

    void BlockGC::_incrementSegment()
    {

        // DEBUG("increment segment\n");
        // DEBUG("seal —— active_segment:%lu, size:%lu, capacity:%lu\n", _active_segment, _segments[_active_segment]._size, _segments[_active_segment]._capacity);
        _sealed_segments.push_back(_active_segment); // 将当前开放段标号加入已写满段列表
        _active_segment = _free_segments.front();    // 切换到下一个开放段
        // DEBUG("active_segment:%lu\n, size:%lu, capacity:%lu\n", _active_segment, _segments[_active_segment]._size, _segments[_active_segment]._capacity);
        _free_segments.pop_front();

        return;
    }

    std::vector<Block> BlockGC::insert(std::vector<Block> items)
    {
        std::vector<Block> evicted;
        for (auto item : items)
        {
            Segment *current_segment = &_segments[_active_segment]; // 当前开放块

            // DEBUG("item.obj_size:%ld\n", item._capacity);

            while (item._capacity + current_segment->_write_point > current_segment->_capacity) // 当前开放块空间不够了
            {
                // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment->_size, current_segment->_capacity);
                // DEBUG("free_segments size:%lu, sealed_segments size:%lu\n", _free_segments.size(), _sealed_segments.size());
                // DEBUG("current total size:%lu, tot_capacity:%lu\n", _total_size, _total_capacity);
                /* move active segment pointer */
                if (_free_segments.empty())
                {
                    // DEBUG("do_gc\n");
                    _do_gc();
                    // DEBUG("current total size:%lu, tot_capacity:%lu\n", _total_size, _total_capacity);
                }
                else
                    _incrementSegment(); // 当前开放块已写满，切换到下一个开放块

                // 在C++中，引用一旦被初始化后，就不能被改变去引用另一个对象；它们必须在声明时被初始化，并且之后不能指向另一个对象。
                // 这意味着，引用一旦绑定到一个对象，就会一直绑定到那个对象，不能被重新绑定。
                current_segment = &_segments[_active_segment];
            }

            _insert(item); // 将对象插入当前开放块
            _log_stats["request_bytes_written"] += item._capacity;
            _log_stats["stores_requested"]++;
            _log_stats["stores_requested_bytes"] += item._capacity; // 存储字节数
        }
        assert(_total_capacity >= _total_size);
        _log_stats["current_size"] = _total_size; // 记录当前缓存对象总大小

        return evicted;
    }

    void BlockGC::update(std::vector<Block> items)
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
                _total_size -= old_item._capacity;
                _segments[it->second]._items.erase(item._lba);
                _item_active.erase(it);
            }
        }
        // 再重新插入
        insert(items);
    }

    void BlockGC::evict(std::vector<uint64_t> items)
    {
        for (auto id : items)
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
                _total_size -= old_item._capacity;
                _segments[it->second]._items.erase(id);
                _item_active.erase(it);
            }
        }
    }

    void BlockGC::readmit(std::vector<Block> items)
    {
        return;
    }

    // 查找对象，只记录了对象是否存在，即只模拟了命中/不命中，没有真正的返回对象
    bool BlockGC::find(Block item)
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

    double BlockGC::ratioCapacityUsed()
    {
        return _total_size / _total_capacity;
    }

    double BlockGC::calcWriteAmp()
    { // 写入的字节数/实际存储的字节数=写放大
        // double ret = _log_stats["bytes_written"] / (double)_log_stats["stores_requested_bytes"];
        // 写入的字节数/用户请求写入的字节数=写放大
        double ret = _log_stats["bytes_written"] / (double)_log_stats["request_bytes_written"];
        return ret;
    }

    void BlockGC::flushStats()
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

    double BlockGC::ratioEvictedToCapacity()
    {
        return _log_stats["sizeEvictions"] / _total_capacity;
    }

} // namespace flashCache
