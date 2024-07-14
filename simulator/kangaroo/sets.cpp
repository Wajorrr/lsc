#include <list>

#include "caches/mem_log_sets_cache.hpp"
#include "constants.hpp"
#include "sets.hpp"
#include "stats/stats.hpp"

namespace flashCache
{

    Sets::Sets(uint64_t num_sets, uint64_t set_capacity,
               stats::LocalStatsCollector &set_stats, cache::MemLogSetsCache *ref_cache,
               int num_hash_functions, bool nru) : _set_stats(set_stats),
                                                   _set_capacity(set_capacity),
                                                   _num_sets(num_sets),
                                                   _total_size(0),
                                                   _total_capacity(set_capacity * num_sets),
                                                   _cache(ref_cache),
                                                   _num_hash_functions(num_hash_functions),
                                                   _nru(nru)
    {
        _bins.resize(_num_sets);
        _set_stats["numSets"] = _num_sets;
        _set_stats["setCapacity"] = _set_capacity;
        _set_stats["numHashFunctions"] = _num_hash_functions;
        _set_stats["nru"] = _nru;
        if (_nru)
        {
            _hits_in_sets.resize(_num_sets);
            for (auto &set_hits : _hits_in_sets)
            {
                set_hits.resize(HIT_BIT_VECTOR_SIZE, false);
            }
        }
    }

    // 向指定集合中插入一个对象
    std::vector<candidate_t> Sets::_insert(candidate_t item, uint64_t bin_num)
    {
        assert((uint64_t)item.obj_size <= _set_capacity);
        std::unordered_set<uint64_t> potential_bins = findSetNums(item); // 可能映射到的集合列表
        assert(potential_bins.find(bin_num) != potential_bins.end());    // 给定的bin_num须在映射到的potential_bins中

        std::vector<candidate_t> evicted;
        assert(!(item.obj_size == 0 && item.id == 0));
        Sets::Bin &bin = _bins[bin_num];
        assert(std::find(bin.items.begin(), bin.items.end(), item) == bin.items.end());
        while ((uint64_t)item.obj_size + bin.bin_size > _set_capacity) // 驱逐直到空间足够容纳当前对象
        {
            // 当前对象没有被命中过，且当前集合全是命中过的对象，则不准入
            if (bin.no_hit_insert_loc == 0 && !item.hit_count)
            {
                _set_stats["numEvictions"]++;
                _set_stats["sizeEvictions"] += item.obj_size;
                _set_stats["numEvictionsImmediate"]++;
                _set_stats["sizeEvictionsImmediate"] += item.obj_size;
                evicted.push_back(item);
                return evicted;
            }
            // 驱逐集合头部的对象
            candidate_t old = bin.items.front();
            _set_stats["numEvictions"]++;
            _set_stats["sizeEvictions"] += old.obj_size;
            bin.bin_size -= old.obj_size;
            _total_size -= old.obj_size;
            if (bin.no_hit_insert_loc > 0) // 驱逐的对象是未命中过的对象
            {
                bin.no_hit_insert_loc--; // 更新未命中对象的插入位置
                evicted.push_back(old);
            }
            else // 驱逐的对象是命中过的对象
            {
                _set_stats["numHitItemsEvicted"]++;
                _set_stats["sizeHitItemsEvicted"] += old.obj_size;
                if (_cache) // 重新写入到日志缓存中
                {
                    _cache->readmitToLogFromSets(old);
                }
                else // 驱逐
                {
                    evicted.push_back(old);
                }
            }
            bin.items.pop_front();
        }

        if (item.hit_count) // 当前对象被访问过
        {
            item.hit_count = 0;
            bin.items.push_back(item); // 放到最后
        }
        else // 当前对象没被访问过
        {
            assert(bin.no_hit_insert_loc <= bin.items.size());
            auto it = bin.items.begin() + bin.no_hit_insert_loc;
            bin.items.insert(it, item); // 放到未命中对象的最后
            bin.no_hit_insert_loc++;    // 更新未命中对象的插入位置
        }
        bin.bin_size += item.obj_size;
        _total_size += item.obj_size;
        _set_stats["current_size"] = _total_size;
        return evicted;
    }

    /* this will result in:
     * 1-a: items w/o hits or items not tracked, b-n items w/ hits
     * returns # of first item with a hit
     */
    uint64_t Sets::_reorder_set_nru(uint64_t bin_num) // 将集合中的对象重新排序，未命中对象放到前面，命中对象放到后面
    {
        assert(bin_num < _num_sets);
        Bin &bin = _bins[bin_num];
        std::vector<bool> &set_hits = _hits_in_sets[bin_num]; // 命中标记
        std::list<candidate_t> hit_items;                     // 命中过的对象
        std::list<candidate_t> no_hit_items;                  // 未命中过的对象
        uint64_t i = 0;
        uint64_t size_hits = 0;
        for (auto const &it : bin.items) // 遍历集合对象
        {
            assert(!(it.obj_size == 0 && it.id == 0));
            if (i >= HIT_BIT_VECTOR_SIZE || !set_hits[i]) // 若当前对象未命中过
            {
                no_hit_items.push_back(it);
            }
            else // 当前对象命中过
            {
                if (_dist_tracking)
                {
                    size_hits += it.obj_size;
                }
                hit_items.push_back(it);
            }
            i++;
        }
        std::fill(set_hits.begin(), set_hits.end(), false); // 重置命中标记
        bin.items.clear();
        bin.items.insert(bin.items.end(), no_hit_items.begin(), no_hit_items.end()); // 未命中对象放到前面
        bin.items.insert(bin.items.end(), hit_items.begin(), hit_items.end());       // 命中对象放到后面
        if (_dist_tracking)
        {
            std::string num_name = "numItemsWithHits" + std::to_string(hit_items.size());
            size_hits = (size_hits / cache::SIZE_BUCKETING) * cache::SIZE_BUCKETING; // 按SIZE_BUCKETING取整
            std::string size_name = "sizeItemsWithHits" + std::to_string(size_hits);
            _set_stats[num_name]++;
            _set_stats[size_name]++;
        }
        return no_hit_items.size();
    }

    // 插入若干对象
    std::vector<candidate_t> Sets::insert(std::vector<candidate_t> items)
    {
        std::vector<bool> sets_touched(_num_sets, false);
        std::vector<candidate_t> evicted;
        for (auto item : items) // 遍历每个要插入的对象
        {
            uint64_t bin_num = *findSetNums(item).begin(); // 对象可能映射到的集合列表，取第一个
            assert(bin_num < _num_sets);
            Bin &bin = _bins[bin_num];
            if (!sets_touched[bin_num] && _nru) // 若当前集合没被记录过，且采用的是NRU策略
            {
                // 集合内对象重排序一下，更新未命中对象的插入位置
                bin.no_hit_insert_loc = _reorder_set_nru(bin_num);
            }
            else if (!sets_touched[bin_num]) // 未采用NRU策略，则默认为FIFO
            {
                // 未命中对象的插入位置为集合末尾
                bin.no_hit_insert_loc = bin.items.size();
            }
            sets_touched[bin_num] = true;
            std::vector<candidate_t> local_evict = _insert(item, bin_num);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
            _updateStatsRequestedStore(item); // 记录请求数以及请求所需写入的字节数
        }
        uint64_t num_sets_touched = std::count(sets_touched.begin(), sets_touched.end(), true);
        _updateStatsActualStore(num_sets_touched); // 记录集合实际写入的字节数
        assert(_total_capacity >= _total_size);
        return evicted;
    }

    // 向指定集合插入若干对象
    std::vector<candidate_t> Sets::insert(uint64_t set_num, std::vector<candidate_t> items)
    {
        std::vector<candidate_t> evicted;
        assert(set_num < _num_sets);
        Bin &bin = _bins[set_num];
        if (_nru)
        {
            bin.no_hit_insert_loc = _reorder_set_nru(set_num);
        }
        else
        {
            bin.no_hit_insert_loc = bin.items.size();
        }
        for (auto item : items)
        {
            std::vector<candidate_t> local_evict = _insert(item, set_num);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
            _updateStatsRequestedStore(item);
        }
        if (items.size() > 0)
        {
            _updateStatsActualStore(1);
        }
        assert(_total_capacity >= _total_size);
        return evicted;
    }

    void Sets::_updateStatsActualStore(uint64_t num_sets_updated)
    {
        _set_stats["bytes_written"] += (num_sets_updated * _set_capacity);
    }

    void Sets::_updateStatsRequestedStore(candidate_t item)
    {
        _set_stats["stores_requested"]++;
        _set_stats["stores_requested_bytes"] += item.obj_size;
    }

    // 查找指定对象
    bool Sets::find(candidate_t item)
    {
        std::unordered_set<uint64_t> possible_bins = findSetNums(item);
        for (uint64_t bin_num : possible_bins)
        {
            uint64_t i = 0;
            for (candidate_t stored_item : _bins[bin_num].items)
            {
                if (stored_item == item)
                {
                    if (_nru) // 若为NRU策略，则更新命中标记，用于NRU的重排序
                    {
                        _hits_in_sets[bin_num][i] = true;
                    }
                    if (_hit_dist) // 按集合粒度统计命中次数
                    {
                        std::string name = "set" + std::to_string(bin_num);
                        _set_stats[name]++;
                    }
                    _set_stats["hits"]++;
                    return true;
                }
                i++;
            }
            if (_hit_dist) // 按集合粒度统计未命中次数
            {
                std::string name = "setMisses" + std::to_string(bin_num);
                _set_stats[name]++;
            }
        }
        _set_stats["misses"]++;
        return false;
    }

    // 对象可能映射到的集合列表
    std::unordered_set<uint64_t> Sets::findSetNums(candidate_t item)
    {
        std::unordered_set<uint64_t> possibilities;
        uint64_t current_value = (uint64_t)item.id;
        for (int i = 0; i <= _num_hash_functions; i++)
        {
            possibilities.insert(current_value % _num_sets);
            current_value = std::hash<std::string>{}(std::to_string(std::hash<uint64_t>{}(current_value)));
        }
        return possibilities;
    }

    double Sets::ratioCapacityUsed()
    {
        return _total_size / (double)_total_capacity;
    }

    double Sets::calcWriteAmp()
    {
        return _set_stats["bytes_written"] / (double)_set_stats["stores_requested_bytes"];
    }

    double Sets::ratioEvictedToCapacity()
    {
        return _set_stats["sizeEvictions"] / (double)_total_capacity;
    }

    void Sets::flushStats()
    {
        _set_stats["misses"] = 0;
        _set_stats["hits"] = 0;
        _set_stats["bytes_written"] = 0;
        _set_stats["stores_requested"] = 0;
        _set_stats["stores_requested_bytes"] = 0;
        _set_stats["sizeEvictions"] = 0;
        _set_stats["numEvictions"] = 0;
        _set_stats["hitsSharedWithLog"] = 0;
        _set_stats["trackHitsFailed"] = 0;
        _set_stats["numHitItemsEvicted"] = 0;
    }

    uint64_t Sets::calcMemoryConsumption()
    {
        /* TODO: replace 0 w/ bloom filters */
        uint64_t sets_memory_consumption = 0;
        if (_nru) // 若为NRU，则为每个对象维护1位命中标记
        {
            // 每个set一个HIT_BIT_VECTOR_SIZE位的命中标记数组
            uint64_t bytes_per_set = HIT_BIT_VECTOR_SIZE / 8;
            sets_memory_consumption = bytes_per_set * _num_sets;
        }
        return sets_memory_consumption;
    }

    // 判断对象是否在集合缓存中
    bool Sets::trackHit(candidate_t item)
    {
        std::unordered_set<uint64_t> possible_bins = findSetNums(item);
        for (uint64_t bin_num : possible_bins)
        {
            uint64_t i = 0;
            for (candidate_t stored_item : _bins[bin_num].items)
            {
                if (stored_item == item)
                {
                    if (_nru)
                    {
                        _hits_in_sets[bin_num][i] = true;
                    }
                    _set_stats["hitsSharedWithLog"]++;
                    return true;
                }
                i++;
            }
        }
        _set_stats["trackHitsFailed"]++;
        return false;
    }

    void Sets::enableDistTracking()
    {
        _dist_tracking = true;
    }

    void Sets::enableHitDistributionOverSets()
    {
        _hit_dist = true;
    }

} // namespace flashCache
