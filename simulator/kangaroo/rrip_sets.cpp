#include <list>
#include <math.h>

#include "caches/mem_log_sets_cache.hpp"
#include "constants.hpp"
#include "rrip_sets.hpp"
#include "stats/stats.hpp"

namespace flashCache
{

    RripSets::RripSets(uint64_t num_sets, uint64_t set_capacity,
                       stats::LocalStatsCollector &set_stats, cache::MemLogSetsCache *ref_cache,
                       int num_hash_functions, int bits, bool promotion_rrip, bool mixed_rrip) : _set_stats(set_stats),
                                                                                                 _set_capacity(set_capacity),
                                                                                                 _num_sets(num_sets),
                                                                                                 _total_size(0),
                                                                                                 _total_capacity(set_capacity * num_sets),
                                                                                                 _cache(ref_cache),
                                                                                                 _num_hash_functions(num_hash_functions),
                                                                                                 _bits(bits),
                                                                                                 _max_rrpv(exp2(bits) - RRIP_DISTANT_DIFF),
                                                                                                 _mixed(mixed_rrip),
                                                                                                 _promotion_only(promotion_rrip)
    {
        _bins.resize(_num_sets);
        _set_stats["numSets"] = _num_sets;                    // 集合数
        _set_stats["setCapacity"] = _set_capacity;            // 每个集合的容量
        _set_stats["numHashFunctions"] = _num_hash_functions; // 哈希函数数量
        _set_stats["rripBits"] = bits;                        // 每个缓存项使用的rrpv位数
    }

    // 将指定集合中所有对象的rrpv值更新为最大rrpv值
    void RripSets::_incrementRrpvValues(uint64_t bin_num)
    {
        Bin &bin = _bins[bin_num]; // 当前缓存集合
        if (bin.rrpv_to_items.empty())
        {
            return;
        }

        /* increment highest key to max rrpv value */
        int current_max = (bin.rrpv_to_items.begin())->first; // 获取当前集合中的最大rrpv值
        int diff = _max_rrpv - current_max;
        if (diff <= 0)
        {
            return;
        }

        auto it = bin.rrpv_to_items.begin();
        while (it != bin.rrpv_to_items.end()) // 遍历所有rrpv值，将各个rrpv值的对象集合全部移动到最大rrpv的集合
        {
            bin.rrpv_to_items[it->first + diff] = it->second;
            it = bin.rrpv_to_items.erase(it);
        }
    }

    // 计算rrip大于insertion_point的所有对象的大小总和
    int64_t RripSets::_calcAllowableSize(Bin &bin, int insertion_point)
    {
        int64_t allowable_size = 0;
        for (auto it = bin.rrpv_to_items.begin(); it != bin.rrpv_to_items.end(); it++)
        {
            if (it->first < insertion_point)
            {
                break;
            }
            // 计算rrip不小于insertion_point的所有对象的大小总和
            allowable_size += std::accumulate(it->second.begin(), it->second.end(), 0,
                                              [](int64_t a, candidate_t b)
                                              { return a + b.obj_size; });
        }
        return allowable_size;
    }

    // 向指定集合中插入一个对象
    std::vector<candidate_t> RripSets::_insert(candidate_t item, uint64_t bin_num)
    {
        assert((uint64_t)item.obj_size <= _set_capacity);
        std::unordered_set<uint64_t> potential_bins = findSetNums(item); // 当前对象可能映射到的集合列表
        assert(potential_bins.find(bin_num) != potential_bins.end());

        std::vector<candidate_t> evicted;
        assert(!(item.obj_size == 0 && item.id == 0));
        Bin &bin = _bins[bin_num]; // 要插入的集合

        // 计算当前对象的rrpv
        int insert_val = _max_rrpv - RRIP_LONG_DIFF - item.hit_count;
        if (insert_val < 0) // 最小为0
        {
            insert_val = 0;
        }
        else if (_promotion_only && item.hit_count) // 如果为promotion_only模式，缓存项只要被再次访问，就将其rrpv更新为0
        {
            insert_val = 0;
        }

        // 若当前对象大小大于已有对象大小之和，且当前集合容量不够容纳当前对象，则不准入当前对象
        if (_calcAllowableSize(bin, insert_val) < item.obj_size && (uint64_t)item.obj_size + bin.bin_size > _set_capacity)
        {
            _set_stats["numEvictions"]++;
            _set_stats["sizeEvictions"] += item.obj_size;
            _set_stats["numEvictionsImmediate"]++;
            _set_stats["sizeEvictionsImmediate"] += item.obj_size;
            if (item.hit_count) // 若当前对象被访问过
            {
                _cache->readmitToLogFromSets(item); // 重新将当前对象插入到日志缓存
            }
            else // 否则驱逐
            {
                evicted.push_back(item);
            }
            return evicted;
        }

        // 若准入当前对象，且集合空间不足，则驱逐直到空间足够容纳当前对象
        while ((uint64_t)item.obj_size + bin.bin_size > _set_capacity)
        {
            assert(!bin.rrpv_to_items.begin()->second.empty());
            candidate_t old = bin.rrpv_to_items.begin()->second.front();
            _set_stats["numEvictions"]++;
            _set_stats["sizeEvictions"] += old.obj_size;
            bin.bin_size -= old.obj_size;
            _total_size -= old.obj_size;
            evicted.push_back(old);
            bin.rrpv_to_items.begin()->second.pop_front();
            if (bin.rrpv_to_items.begin()->second.empty()) // 若当前rrpv集合中的对象已经被驱逐完了
            {
                // 当你从std::map中删除一个项时，该项的键和值都会被销毁
                bin.rrpv_to_items.erase(bin.rrpv_to_items.begin()); // 删除当前rrpv对应的集合，自动遍历下一个集合
            }
        }
        // 在C++的std::map中，如果你尝试访问一个不存在的键，会自动创建一个新的键值对，键是你提供的键，值是该类型的默认值
        bin.rrpv_to_items[insert_val].push_back(item);
        bin.bin_size += item.obj_size; // 当前集合中对象总大小
        _total_size += item.obj_size;  // 当前所有对象总大小
        _set_stats["current_size"] = _total_size;
        return evicted;
    }

    // 插入一批对象
    std::vector<candidate_t> RripSets::insert(std::vector<candidate_t> items)
    {
        std::vector<bool> sets_touched(_num_sets, false);
        std::vector<candidate_t> evicted;
        for (auto item : items) // 遍历要插入的对象
        {
            uint64_t bin_num = *findSetNums(item).begin(); // 选取当前对象可能映射到的集合中的第一个
            assert(bin_num < _num_sets);
            if (!sets_touched[bin_num]) // 当前集合没被访问过
            {
                _incrementRrpvValues(bin_num);
            }
            sets_touched[bin_num] = true;
            std::vector<candidate_t> local_evict = _insert(item, bin_num);         // 将对象插入到当前集合，获取驱逐对象列表
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end()); // 将驱逐对象列表添加到总的驱逐对象列表中
            _updateStatsRequestedStore(item);                                      // 更新存储的请求数量和总字节数
        }
        // 插入所有对象后，共插入到了多少个集合
        uint64_t num_sets_touched = std::count(sets_touched.begin(), sets_touched.end(), true);
        _updateStatsActualStore(num_sets_touched); // 由于更新需要对整个集合进行重写，更新实际写入的字节数，即更新的集合数乘以集合容量
        assert(_total_capacity >= _total_size);
        return evicted; // 返回驱逐对象列表
    }

    // 向指定集合插入一批对象
    std::vector<candidate_t> RripSets::insert(uint64_t set_num, std::vector<candidate_t> items)
    {
        std::vector<candidate_t> evicted;
        assert(set_num < _num_sets);
        _incrementRrpvValues(set_num);
        for (auto item : items) // 遍历要插入的对象
        {
            std::vector<candidate_t> local_evict = _insert(item, set_num);
            evicted.insert(evicted.end(), local_evict.begin(), local_evict.end());
            _updateStatsRequestedStore(item);
        }
        if (items.size() > 0)
        {
            _updateStatsActualStore(1); // 只更新了1个集合，实际写入量为一个集合的容量
        }
        assert(_total_capacity >= _total_size);
        return evicted;
    }

    // 更新实际的写入字节数，即当前请求写入了多少个集合，需要对整个集合进行重写
    void RripSets::_updateStatsActualStore(uint64_t num_sets_updated)
    {
        _set_stats["bytes_written"] += (num_sets_updated * _set_capacity);
    }

    // 更新请求指定的写入字节数
    void RripSets::_updateStatsRequestedStore(candidate_t item)
    {
        _set_stats["stores_requested"]++;
        _set_stats["stores_requested_bytes"] += item.obj_size;
    }

    // 查找一个对象
    bool RripSets::find(candidate_t item)
    {
        std::unordered_set<uint64_t> possible_bins = findSetNums(item); // 对象可能映射到的集合列表
        for (uint64_t bin_num : possible_bins)
        { // 遍历可能映射到的集合
            auto it = _bins[bin_num].rrpv_to_items.begin();
            // 遍历当前集合中的所有rrpv值对应的deque
            while (it != _bins[bin_num].rrpv_to_items.end())
            {
                // 遍历deque中的各个对象
                for (auto vec_it = it->second.begin(); vec_it != it->second.end(); vec_it++)
                {
                    if (*vec_it == item) // 找到了当前对象
                    {
                        if (it->first != 0) // 若对象所属的rrpv不为0
                        {
                            if (_mixed || _promotion_only)
                            { // 将对象的rrpv更新为0
                                _bins[bin_num].rrpv_to_items[0].push_back(*vec_it);
                            }
                            else
                            { // 将对象的rrpv减1
                                _bins[bin_num].rrpv_to_items[it->first - 1].push_back(*vec_it);
                            }
                            it->second.erase(vec_it); // 将对象从原rrpv的deque中删除
                            if (it->second.empty())   // 若原rrpv的deque为空，则erase
                            {
                                _bins[bin_num].rrpv_to_items.erase(it);
                            }
                        }
                        _set_stats["hits"]++;
                        if (_hit_dist) // 按集合粒度统计的hit/miss分布
                        {
                            std::string name = "setHits" + std::to_string(bin_num);
                            _set_stats[name]++;
                        }
                        return true;
                    }
                }
                it++;
            }
            if (_hit_dist) // 按集合粒度统计的hit/miss分布  // 这里是只要对集合进行了遍历读取且没找到要找的对象，就算miss
            {
                std::string name = "setMisses" + std::to_string(bin_num);
                _set_stats[name]++;
            }
        }
        // 没找到
        _set_stats["misses"]++;
        return false;
    }

    // 获取当前对象能够映射到的集合列表
    std::unordered_set<uint64_t> RripSets::findSetNums(candidate_t item)
    {
        std::unordered_set<uint64_t> possibilities;
        uint64_t current_value = (uint64_t)item.id;    // 对象id
        for (int i = 0; i <= _num_hash_functions; i++) // 根据哈希函数数量，计算当前对象能够映射到的各个集合
        {
            // 计算当前哈希值取模之后映射到的集合编号
            possibilities.insert(current_value % _num_sets);
            // 将当前哈希值使用std::hash、转string之后再次hash，作为下一次迭代的哈希值
            current_value = std::hash<std::string>{}(std::to_string(std::hash<uint64_t>{}(current_value)));
        }
        return possibilities;
    }

    double RripSets::ratioCapacityUsed()
    {
        return _total_size / (double)_total_capacity;
    }

    double RripSets::calcWriteAmp()
    {
        return _set_stats["bytes_written"] / (double)_set_stats["stores_requested_bytes"];
    }

    double RripSets::ratioEvictedToCapacity()
    {
        return _set_stats["sizeEvictions"] / (double)_total_capacity;
    }

    void RripSets::flushStats()
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

    // 估计set缓存的内存占用，主要是为每个对象所维护的rrpv元数据开销
    uint64_t RripSets::calcMemoryConsumption()
    {
        /* TODO: add bloom filters */
        /* this is a strong estimate based on avg obj_size */
        // 每个集合使用的bit数 = 每个缓存项使用的bit数 * 每个集合中平均对象数量，直接使用了平均对象大小来进行统计
        double bits_per_set = _bits * (_set_capacity / AVG_OBJ_SIZE_BYTES);
        // 总内存占用 = 每个集合使用的bit数 * 集合数
        uint64_t sets_memory_consumption_bits = (uint64_t)bits_per_set * _num_sets;
        return sets_memory_consumption_bits / 8; // 单位转换为字节
    }

    // 跟find差不多，是在log中未命中，在set中再判断一下是否命中吗？
    bool RripSets::trackHit(candidate_t item)
    {
        std::unordered_set<uint64_t> possible_bins = findSetNums(item);
        for (uint64_t bin_num : possible_bins)
        {
            auto it = _bins[bin_num].rrpv_to_items.begin();
            while (it != _bins[bin_num].rrpv_to_items.end())
            {
                for (auto vec_it = it->second.begin(); vec_it != it->second.end(); vec_it++)
                {
                    if (*vec_it == item)
                    {
                        if (it->first != 0)
                        {
                            if (_mixed || _promotion_only)
                            {
                                _bins[bin_num].rrpv_to_items[0].push_back(*vec_it);
                            }
                            else
                            {
                                _bins[bin_num].rrpv_to_items[it->first - 1].push_back(*vec_it);
                            }
                            it->second.erase(vec_it);
                            if (it->second.empty())
                            {
                                _bins[bin_num].rrpv_to_items.erase(it);
                            }
                        }
                        _set_stats["hitsSharedWithLog"]++;
                        return true;
                    }
                }
                it++;
            }
        }
        _set_stats["trackHitsFailed"]++;
        return false;
    }

    void RripSets::enableDistTracking()
    {
        _dist_tracking = true;
    }

    void RripSets::enableHitDistributionOverSets()
    {
        _hit_dist = true;
    }

} // namespace flashCache
