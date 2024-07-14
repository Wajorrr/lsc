#include "constants.hpp"
#include "fifo_log.hpp"
#include "stats/stats.hpp"
#include "common/logging.h"

namespace flashCache
{

    FIFOLog::FIFOLog(uint64_t log_capacity, uint64_t block_size,
                     stats::LocalStatsCollector &log_stats) : _log_stats(log_stats),
                                                              _total_capacity(log_capacity),
                                                              _total_size(0),
                                                              _active_block(0),
                                                              _readmit(0)
    {
        _log_stats["logCapacity"] = _total_capacity;
        _num_blocks = log_capacity / block_size; // 包含的擦除块数量
        Block template_block = Block(block_size);
        _blocks.resize(_num_blocks, template_block); // 根据擦除块的数量和大小来分配擦除块数组空间
        // allow last block to be smaller than the others
        if (log_capacity % block_size) // 剩余空间少于一个标准擦除块大小
        {
            _num_blocks++;
            template_block._capacity = log_capacity % block_size; // 创建一个单独的擦除块
            _blocks.push_back(template_block);
        }
        std::cout << "Log capacity: " << _total_capacity
                  << "\n\tNum Blocks: " << _num_blocks
                  << "\n\tBlock Capacity: " << block_size << std::endl;
    }

    void FIFOLog::_insert(candidate_t item)
    {
        _log_stats["bytes_written"] += item.obj_size; // 写入字节数
        _log_stats["request_bytes_written"] += item.obj_size;
        _log_stats["stores_requested"]++;
        _log_stats["stores_requested_bytes"] += item.obj_size;    // 存储字节数
        assert(_item_active.find(item.id) == _item_active.end()); // 保证对象在当前flash Cache中不存在
        _total_size += item.obj_size;
        item.hit_count = 0;
        _blocks[_active_block].insert(item); // 将对象插入当前开放的擦除块中
        _item_active[item.id] = _active_block;
        assert(_blocks[_active_block]._size <= _blocks[_active_block]._capacity);
        // _num_inserts++;
        // _size_inserts += item.obj_size;
        // assert(_size_inserts == _blocks[_active_block]._size);
    }

    std::vector<candidate_t> FIFOLog::_incrementBlockAndFlush()
    {
        std::vector<candidate_t> evicted;
        _active_block = (_active_block + 1) % _num_blocks; // 新的开放块
        Block &current_block = _blocks[_active_block];

        if (current_block._size) // 如果当前擦除块中有数据
        {
            // 将当前擦除块中的有效对象加入驱逐列表
            evicted.reserve(current_block._items.size());
            for (auto [id, item] : current_block._items)
            {
                if (_item_active.find(item.id) != _item_active.end())
                {
                    // only move if not already in sets
                    evicted.push_back(item);
                }
                // should always remove an item, otherwise code bug
                _item_active.erase(item.id);
            }
            _log_stats["numEvictions"] += current_block._items.size();
            _log_stats["sizeEvictions"] += current_block._size;
            _log_stats["numLogFlushes"]++;
            _total_size -= current_block._size;
            current_block._items.clear();
            current_block._size = 0;
        }
        // _num_inserts = 0;
        // _size_inserts = 0;
        return evicted;
    }

    std::vector<candidate_t> FIFOLog::insert(std::vector<candidate_t> items)
    {
        std::vector<candidate_t> evicted;
        for (auto item : items)
        {
            Block &current_block = _blocks[_active_block]; // 当前开放块
            // DEBUG("current_block._size:%ld,current_block._capacity:%ld\n", current_block._size, current_block._capacity);
            // DEBUG("item.obj_size:%ld\n", item.obj_size);
            if (item.obj_size + current_block._size > current_block._capacity) // 当前开放块空间不够了
            {
                /* move active block pointer */
                std::vector<candidate_t> local_evict = _incrementBlockAndFlush();      // 打开一个新的开放块，可能需要驱逐一些对象
                evicted.insert(evicted.end(), local_evict.begin(), local_evict.end()); // 将驱逐的对象加入驱逐列表
            }
            _insert(item); // 将对象插入当前开放块
        }
        assert(_total_capacity >= _total_size);
        _log_stats["current_size"] = _total_size; // 记录当前缓存对象总大小
        return evicted;
    }

    void FIFOLog::update(std::vector<candidate_t> items)
    {
        for (auto item : items)
        {
            // _log_stats["stores_requested"]++;
            _log_stats["request_bytes_written"] += item.obj_size;
            _log_stats["bytes_written"] += item.obj_size;
            auto it = _item_active.find(item.id);
            if (it != _item_active.end())
            {
                auto old_item = _blocks[it->second]._items[item.id];
                _blocks[it->second]._size -= old_item.obj_size;
                _log_stats["stores_requested_bytes"] -= old_item.obj_size;
                _total_size -= old_item.obj_size;
                _blocks[it->second]._items.erase(item.id);
                _item_active.erase(it);
            }
        }
        insert(items);
    }

    void FIFOLog::readmit(std::vector<candidate_t> items)
    {
        return;
    }

    void FIFOLog::insertFromSets(candidate_t item)
    {
        return;
    }

    // 查找对象，只记录了对象是否存在，即只模拟了命中/不命中，没有真正的返回对象
    bool FIFOLog::find(candidate_t item)
    {
        auto it = _item_active.find(item.id);
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

    double FIFOLog::ratioCapacityUsed()
    {
        return _total_size / _total_capacity;
    }

    double FIFOLog::calcWriteAmp()
    { // 写入的字节数/实际存储的字节数=写放大
        // double ret = _log_stats["bytes_written"] / (double)_log_stats["stores_requested_bytes"];
        // 写入的字节数/用户请求写入的字节数=写放大
        double ret = _log_stats["bytes_written"] / (double)_log_stats["request_bytes_written"];
        return ret;
    }

    void FIFOLog::flushStats()
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

    double FIFOLog::ratioEvictedToCapacity()
    {
        return _log_stats["sizeEvictions"] / _total_capacity;
    }

} // namespace flashCache
