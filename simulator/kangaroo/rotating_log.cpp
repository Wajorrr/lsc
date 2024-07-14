#include "constants.hpp"
#include "rotating_log.hpp"
#include "stats/stats.hpp"

namespace flashCache
{

    // 添加了一个set逻辑结构，表示日志结构中各个对象所映射到的集合，在驱逐到集合缓存时会将其驱逐到对应的集合中
    RotatingLog::RotatingLog(uint64_t log_capacity, uint64_t block_size, SetsAbstract *sets,
                             stats::LocalStatsCollector &log_stats, uint64_t readmit) : _sets(sets),
                                                                                        _log_stats(log_stats),
                                                                                        _total_capacity(log_capacity),
                                                                                        _total_size(0),
                                                                                        _active_block(0),
                                                                                        _readmit(readmit)
    {
        _log_stats["logCapacity"] = _total_capacity; // 日志缓存容量
        _num_blocks = log_capacity / block_size;     // 擦除块数量
        Block template_block = Block(block_size);
        _blocks.resize(_num_blocks, template_block);
        // allow last block to be smaller than the others
        if (log_capacity % block_size)
        {
            _num_blocks++;
            template_block._capacity = log_capacity % block_size;
            _blocks.push_back(template_block);
        }
    }

    // 插入一个对象
    void RotatingLog::_insert(candidate_t item)
    {
        _log_stats["bytes_written"] += item.obj_size;
        _log_stats["stores_requested"]++;
        _log_stats["stores_requested_bytes"] += item.obj_size;
        _total_size += item.obj_size;
        item.hit_count = 0;
        _blocks[_active_block].insert(item); // 向开放块中插入对象
        _per_item_hits[item] = 0;
        if (_sets) // 若有set
        {
            uint64_t set_num = *(_sets->findSetNums(item).begin()); // 找到当前对象映射到的第一个集合
            _set_to_items[set_num].push_back(item);                 // 在相应的集合中加入当前对象
        }
        _item_active[item] = true;
        assert(_blocks[_active_block]._size <= _blocks[_active_block]._capacity);
    }

    // 打开一个新开放块，驱逐其中所有有效对象
    std::vector<candidate_t> RotatingLog::_incrementBlockAndFlush()
    {
        std::vector<candidate_t> evicted;
        _active_block = (_active_block + 1) % _num_blocks; // 下一个开放块
        Block &current_block = _blocks[_active_block];

        if (current_block._size) // 若当前开放块不为空，则先驱逐其中所有对象
        {
            evicted.reserve(current_block._items.size());
            for (auto item : current_block._items)
            {
                if (_item_active[item])
                {
                    // only move if not already in sets
                    evicted.push_back(item);
                }
                // should always remove an item, otherwise code bug
                _item_active.erase(item);
            }
            _log_stats["numEvictions"] += current_block._items.size();
            _log_stats["sizeEvictions"] += current_block._size;
            _log_stats["numLogFlushes"]++;
            _total_size -= current_block._size;
            current_block._items.clear();
            current_block._size = 0;
        }
        return evicted;
    }

    //
    std::vector<candidate_t> RotatingLog::_addSetMatches(std::vector<candidate_t> evicted)
    {
        if (_sets == nullptr)
        {
            return evicted;
        }
        std::vector<candidate_t> ret; // 要驱逐到集合缓存的对象
        for (auto item : evicted)     // 遍历要驱逐的对象
        {
            uint64_t set_num = *(_sets->findSetNums(item).begin()); // 当前对象映射到的集合
            /* if already in set_indices, already moved over */
            if (_set_to_items[set_num].size())
            {
                ret.reserve(ret.size() + _set_to_items[set_num].size()); // 预留空间
                uint64_t size_moved = 0;
                std::vector<candidate_t> not_moved;
                for (auto item_evicted : _set_to_items[set_num]) // 遍历日志缓存中和当前对象属于同一集合的对象
                {
                    // 判断当前对象是否为有效对象，即是否已经被驱逐
                    bool already_evicted = _item_active.find(item_evicted) == _item_active.end();
                    // 若当前对象有效，但要移动的对象大小已经超过阈值
                    if (!already_evicted && size_moved > EVICT_SET_LIMIT)
                    {
                        not_moved.push_back(item_evicted); // 不移动
                        continue;
                    }
                    item_evicted.hit_count = _per_item_hits[item_evicted]; // 当前对象的命中次数
                    size_moved += item_evicted.obj_size;                   // 移动对象的总大小
                    ret.push_back(item_evicted);                           // 驱逐

                    if (!already_evicted) // 若当前对象有效
                    {
                        // do not update stats for items force evicted
                        _log_stats["num_early_evict"]++;
                        _log_stats["size_early_evict"] += item_evicted.obj_size;
                        _item_active[item_evicted] = false;
                    }
                }
                _set_to_items[set_num] = not_moved;
            }
        }
        return ret;
    }

    std::vector<candidate_t> RotatingLog::insert(std::vector<candidate_t> items)
    {
        std::vector<candidate_t> evicted;
        for (auto item : items)
        {
            Block &current_block = _blocks[_active_block];
            if (item.obj_size + current_block._size > current_block._capacity) // 当前开放块不够容纳当前对象
            {
                /* move active block pointer */
                std::vector<candidate_t> local_evict = _incrementBlockAndFlush(); // 打开新的开放块，驱逐其中的对象
                evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
            }
            _insert(item); // 在新的开放块中插入对象
        }
        evicted = _addSetMatches(evicted); // 将与驱逐对象同一集合的对象加入到驱逐列表中
        assert(_total_capacity >= _total_size);
        _log_stats["current_size"] = _total_size;
        return evicted;
    }

    void RotatingLog::insertFromSets(candidate_t item)
    {
        uint64_t set_num = *(_sets->findSetNums(item).begin()); // 当前对象映射到的集合
        if (_item_active.find(item) != _item_active.end())      // 对象有效，则不用再次插入到开放块中
        {
            _log_stats["num_early_evict"]--;                 // 撤销驱逐
            _log_stats["size_early_evict"] -= item.obj_size; // 撤销驱逐
            _item_active[item] = true;
            _set_to_items[set_num].push_back(item); // 在相应的集合中加入当前对象
            return;
        }
        Block &current_block = _blocks[_active_block]; // 当前开放块
        if (item.obj_size + current_block._size > current_block._capacity)
        { // 空间不够容纳，驱逐出flash cache
            _log_stats["bytes_rejected_from_sets"] += item.obj_size;
            _log_stats["num_rejected_from_sets"]++;
            return;
        }
        // 重新插入到当前开放块中
        _log_stats["bytes_readmitted"] += item.obj_size;
        _log_stats["num_readmitted"]++;
        _log_stats["bytes_written"] += item.obj_size;
        _total_size += item.obj_size;
        current_block.insert(item);
        _item_active[item] = true;
        _set_to_items[set_num].push_back(item);
        _per_item_hits[item] = 0;
    }

    void RotatingLog::readmit(std::vector<candidate_t> items)
    {
        for (auto item : items)
        {
            uint64_t set_num = *(_sets->findSetNums(item).begin());
            if (_item_active.find(item) != _item_active.end()) // 对象有效
            {
                _log_stats["num_early_evict"]--;
                _log_stats["size_early_evict"] -= item.obj_size;
                _item_active[item] = true;
                _set_to_items[set_num].push_back(item);
            }
            else if (_readmit && _per_item_hits[item] > _readmit) // 对象访问次数超过了准入阈值
            {
                // 当前开放块空间不够，将当前对象驱逐出flash cache
                if (_blocks[_active_block]._size + item.obj_size > _blocks[_active_block]._capacity)
                {
                    /* don't want to clean another block, could be back in this mess */
                    _log_stats["readmit_evicted"]++;
                    _log_stats["readmit_evicted_size"] += item.obj_size;
                    _per_item_hits.erase(item);
                    continue;
                }
                _log_stats["bytes_readmitted"] += item.obj_size;
                _log_stats["num_readmitted"]++;
                _log_stats["bytes_written"] += item.obj_size;
                _set_to_items[set_num].push_back(item);
                _total_size += item.obj_size;
                _blocks[_active_block].insert(item);
                _per_item_hits[item] = 0;
                _item_active[item] = true;
            }
            _per_item_hits.erase(item);
        }
        _log_stats["current_size"] = _total_size;
        assert(_total_capacity >= _total_size);
    }

    bool RotatingLog::find(candidate_t item)
    {
        auto it = _item_active.find(item);
        if (it == _item_active.end())
        {
            _log_stats["misses"]++;
            return false;
        }
        else if (!(it->second)) // 若在日志缓存中没找到
        {
            _log_stats["hits"]++;
            // pass along for hit tracking in sets
            bool found = _sets->trackHit(item); // 在集合缓存中找
            if (!found)
            {
                it->second = true;
            }
            return true;
        }
        else
        {
            _log_stats["hits"]++;
            _per_item_hits[item]++;
            return true;
        }
    }

    double RotatingLog::ratioCapacityUsed()
    {
        return _total_size / _total_capacity;
    }

    double RotatingLog::calcWriteAmp()
    {
        double ret = _log_stats["bytes_written"] / (double)_log_stats["stores_requested_bytes"];
        return ret;
    }

    void RotatingLog::flushStats()
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

} // namespace flashCache
