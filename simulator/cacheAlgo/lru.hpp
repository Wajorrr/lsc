#pragma once

#include <unordered_map>
#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "eviction_algo_base.h"
#include "common/logging.h"

namespace CacheAlgo
{
    struct Tags
        : public std::unordered_map<obj_id_t, cache_obj_t *> // 对象到链表节点的映射
    {
        cache_obj_t *lookup(obj_id_t id) const
        {
            auto itr = this->find(id); // 在hash表中查找对象
            if (itr != this->end())
            {
                return itr->second;
            }
            else
            {
                return nullptr;
            }
        }

        cache_obj_t *allocate(obj_id_t id, int64_t size) // 为对象创建一个新链表节点
        {
            auto *entry = new cache_obj_t(id, size);
            (*this)[id] = entry;
            return entry;
        }

        cache_obj_t *evict(obj_id_t id) // 驱逐对象
        {
            auto itr = this->find(id);
            assert(itr != this->end());

            auto *entry = itr->second;
            this->erase(itr); // 删除对象索引
            return entry;     // 返回链表节点
        }
    };

    class LRU : public virtual CacheAlgoAbstract
    {
    public:
        LRU(uint64_t _max_size, stats::LocalStatsCollector &lru_stats)
            : max_size(_max_size), current_size(0), _lru_stats(lru_stats)
        {
            _lru_stats["lruCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            lru_head = nullptr;
            lru_tail = nullptr;
        }

        std::vector<uint64_t> insert(obj_id_t id, int64_t size) // 插入新对象
        {
            std::vector<uint64_t> evicted;
            if ((uint64_t)size > max_size) // 不准入
            {
                WARN("Object size exceeds cache capacity, object size: {}, cache capacity: {}", size, max_size);
                _lru_stats["numEvictions"]++;
                _lru_stats["sizeEvictions"] += size;
                evicted.push_back(id);
                return evicted;
            }

            while (current_size + size > max_size) // 驱逐直到空间足够容纳当前对象
            {
                cache_obj_t *evict_id = lru_tail; // 驱逐LRU尾部对象
                _lru_stats["numEvictions"]++;
                _lru_stats["sizeEvictions"] += evict_id->obj_size;
                evicted.push_back(evict_id->obj_id);
                replaced(evict_id->obj_id);
            }
            cache_obj_t *new_entry = tags.allocate(id, size);     // 为对象创建一个新链表节点
            prepend_obj_to_head(&lru_head, &lru_tail, new_entry); // 插入头部
            current_size += size;
            _lru_stats["current_size"] = current_size;
            assert(current_size <= max_size);
            return evicted;
        }

        void replaced(obj_id_t id) // 驱逐给定的对象
        {
            auto *entry = tags.evict(id);                      // 先删除索引
            current_size -= entry->obj_size;                   // 当前缓存大小
            remove_obj_from_list(&lru_head, &lru_tail, entry); // 再删除链表节点
            free_cache_obj(entry);
        }

        bool get(obj_id_t id) // 读请求一个对象
        {
            auto *entry = tags.lookup(id);
            if ((bool)entry)
            {
                move_obj_to_head(&lru_head, &lru_tail, entry);
                _lru_stats["hits"]++;
            }
            else
            {
                _lru_stats["misses"]++;
            }
            return (bool)entry;
        }

        std::vector<uint64_t> set(obj_id_t id, int64_t size) // 读请求miss后插入一个对象，或者写请求写入/更新一个对象
        {
            auto *entry = tags.lookup(id);
            if ((bool)entry) // 对象已经存在，更新(set一个已经存在的对象)
            {
                replaced(id);            // 先驱逐原记录
                return insert(id, size); // 再插入新记录
            }
            else // 对象不存在，插入(get miss时或set一个新对象)
            {
                return insert(id, size);
            }
        }

        uint64_t get_current_size() const
        {
            return current_size;
        }

    private:
        cache_obj_t *lru_head;
        cache_obj_t *lru_tail;
        Tags tags; // 一个hash表
        uint64_t max_size;
        uint64_t current_size;
        stats::LocalStatsCollector &_lru_stats;
    };

} // namespace CacheAlgo
