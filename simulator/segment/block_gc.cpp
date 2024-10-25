#include "constants.hpp"
#include "block_gc.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    BlockGC::BlockGC(uint64_t _log_capacity, stats::LocalStatsCollector &_log_stats)
        : BlockLogAbstract(_log_capacity, _log_stats)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_segments = _log_capacity / Segment::_capacity; // 包含的段数量

        // Segment::_capacity = Segment::_capacity;
        // Segment template_segment = Segment();
        // _segments.resize(_num_segments, template_segment); // 根据段的数量和大小来分配段数组空间
        init_segments();

        // allow last segment to be smaller than the others
        // GC模式下，对齐到段大小，多出来的空间舍弃
        if (_log_capacity % Segment::_capacity) // 剩余空间少于一个标准段大小
        {
            _total_capacity -= _log_capacity % Segment::_capacity;
            _log_stats["logCapacity"] = _total_capacity;
            WARN("totCapacity:%ld, segment num:%d, rest capacity:%ld, align to %ld\n",
                 _log_capacity, _num_segments, _log_capacity % Segment::_capacity, _total_capacity);

            // _num_segments++;
            // template_segment._capacity = _log_capacity % Segment::_capacity; // 创建一个单独的段
            // _segments.push_back(template_segment);
        }

        // 初始化_sealed_segments为空
        _sealed_segments.clear();

        // 初始化_free_segments为1到_num_segments-1(0为默认初始开放段)
        for (int i = 1; i < _num_segments; ++i)
        {
            _free_segments.push_back(i);
        }

        DEBUG("Log capacity: %ld, Num Segments: %d, Segment Capacity: %ld\n",
              _total_capacity, _num_segments, Segment::_capacity);
        // std::cout << "Log capacity: " << _total_capacity
        //           << "\n\tNum Segments: " << _num_segments
        //           << "\n\tSegment Capacity: " << Segment::_capacity << std::endl;
    }

    uint32_t BlockGC::_victim_select()
    {
        // INFO("_free_segments.size():%lu\n", _free_segments.size());
        // INFO("_sealed_segments.size():%lu\n", _sealed_segments.size());
        auto min_it = std::min_element(_sealed_segments.begin(), _sealed_segments.end(),
                                       [this](const uint32_t &a, const uint32_t &b)
                                       {
                                           return _segments[a]->_size < _segments[b]->_size;
                                       });
        uint32_t min_idx = *min_it;
        _sealed_segments.erase(min_it);
        return min_idx;
    }

    void BlockGC::_do_gc()
    {
        uint64_t total_reclaimed = 0;
        std::vector<Block> rewrite_blocks;
        // DEBUG("before select: free_segments size:%lu, sealed_segments size:%lu\n", _free_segments.size(), _sealed_segments.size());
        // 选取若干个段GC，保证GC回收的空间不小于一个段的大小
        while (total_reclaimed < Segment::_capacity)
        {
            uint32_t victim_idx = _victim_select(); // 选择一个段进行GC
            // DEBUG("victim_idx:%u\n", victim_idx);
            Segment &victim = *_segments[victim_idx];
            uint64_t total_rewrite = 0;
            for (auto &item : victim._items)
            {
                rewrite_blocks.push_back(item.second);
                _item_active.erase(item.first);
                total_rewrite += item.second._capacity;
            }
            _current_size -= victim._size;
            total_reclaimed += Segment::_capacity - victim._size;
            // DEBUG("total_reclaimed:%lu, victim._size:%lu, segment._capacity:%lu\n",
            //       total_reclaimed, victim._size, Segment::_capacity);
            victim.reset();
            _free_segments.push_back(victim_idx); // 将被GC的段加入空闲段列表
        }
        // DEBUG("after select: free_segments size:%lu, sealed_segments size:%lu\n", _free_segments.size(), _sealed_segments.size());
        // 有效数据迁移
        for (auto &item : rewrite_blocks)
        {
            Segment &current_segment = *_segments[_active_segment]; // 当前开放块
            // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment._size, current_segment._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item._capacity + current_segment._write_point > current_segment._capacity) // 当前开放块空间不够了
            {
                _incrementSegment(); // 当前开放块已写满，切换到下一个开放块
                // DEBUG("rewrite: free_segments size:%lu, sealed_segments size:%lu\n", _free_segments.size(), _sealed_segments.size());
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
    //     DEBUG("temptotSize:%lu, _current_size:%lu\n", temptotSize, _current_size);
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
        for (auto &item : items)
        {
            // Segment *current_segment = &_segments[_active_segment]; // 当前开放块
            Segment *current_segment = _segments[_active_segment]; // 当前开放块

            // DEBUG("item.obj_size:%ld\n", item._capacity);

            while (item._capacity + current_segment->_write_point > current_segment->_capacity) // 当前开放块空间不够了
            {
                // DEBUG("current_segment._size:%ld,current_segment._capacity:%ld\n", current_segment->_size, current_segment->_capacity);
                // DEBUG("free_segments size:%lu, sealed_segments size:%lu\n", _free_segments.size(), _sealed_segments.size());
                // DEBUG("current total size:%lu, tot_capacity:%lu\n", _current_size, _total_capacity);
                /* move active segment pointer */
                if (_free_segments.empty())
                {
                    // 先把当前开放段加入_sealed_segments中，保证所有段均在sealed_segments中
                    _sealed_segments.push_back(_active_segment);
                    DEBUG("do gc\n");
                    DEBUG("current total size:%lu, tot_capacity:%lu\n", _current_size, _total_capacity);
                    _do_gc();
                    DEBUG("current total size:%lu, tot_capacity:%lu\n", _current_size, _total_capacity);
                }
                else
                    _incrementSegment(); // 当前开放块已写满，切换到下一个开放块

                // 在C++中，引用一旦被初始化后，就不能被改变去引用另一个对象；它们必须在声明时被初始化，并且之后不能指向另一个对象。
                // 这意味着，引用一旦绑定到一个对象，就会一直绑定到那个对象，不能被重新绑定。
                // current_segment = &_segments[_active_segment];
                current_segment = _segments[_active_segment];
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
