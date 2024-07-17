#pragma once

#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "eviction_algo_base.h"
#include "common/logging.h"
#include "tags.hpp"

namespace CacheAlgo
{
    class LRU : public virtual CacheAlgoAbstract
    {
    public:
        LRU(uint64_t _max_size, stats::LocalStatsCollector &lru_stats)
            : CacheAlgoAbstract("LRU", _max_size, lru_stats)
        {
            cache_algo_stats["lruCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            lru_head = nullptr;
            lru_tail = nullptr;
        }

        bool get(const parser::Request *req) // 读请求一个对象
        {
            bool hit = cache_get_base(req);
            if (hit)
            {
                cache_algo_stats["hits"]++;
            }
            else
            {
                cache_algo_stats["misses"]++;
            }
            return hit;
        }

        cache_obj_t *find(const parser::Request *req, const bool update_cache)
        {
            cache_obj_t *cache_obj = cache_find_base(req, update_cache);
            if (cache_obj && likely(update_cache))
            {
                /* lru_head is the newest, move cur obj to lru_head */
#ifdef USE_BELADY
                if (req->next_access_vtime != INT64_MAX)
#endif
                    move_obj_to_head(&lru_head, &lru_tail, cache_obj);
            }
            return cache_obj;
        }

        cache_obj_t *insert(const parser::Request *req)
        {
            cache_obj_t *obj = cache_insert_base(req);
            prepend_obj_to_head(&lru_head, &lru_tail, obj);

            return obj;
        }

        cache_obj_t *to_evict(const parser::Request *req)
        {
            DEBUG_ASSERT(lru_tail != NULL || current_size == 0);

            to_evict_candidate_gen_vtime = req_num;
            return lru_tail;
        }

        void evict(const parser::Request *req)
        {
            cache_obj_t *obj_to_evict = lru_tail;
            DEBUG_ASSERT(lru_tail != NULL);

            // we can simply call remove_obj_from_list here, but for the best performance,
            // we chose to do it manually
            // remove_obj_from_list(&lru_head, &lru_tail, obj)

            lru_tail = lru_tail->queue.prev;
            if (likely(lru_tail != NULL))
            {
                lru_tail->queue.next = NULL;
            }
            else
            {
                /* obj_num has not been updated */
                DEBUG_ASSERT(obj_num == 1);
                lru_head = NULL;
            }

#if defined(TRACK_DEMOTION)
            if (cache->track_demotion)
                printf("%ld demote %ld %ld\n", req_num, obj_to_evict->create_time,
                       obj_to_evict->misc.next_access_vtime);
#endif

            cache_evict_base(obj_to_evict, true);
        }

        bool remove(const obj_id_t id)
        {
            cache_obj_t *obj = tags.hashtable_find_obj_id(id);
            if (obj == NULL)
            {
                return false;
            }

            remove_obj_from_list(&lru_head, &lru_tail, obj);
            cache_remove_obj_base(obj, true);

            return true;
        }

        void print_cache()
        {
            cache_obj_t *cur = lru_head;
            // print from the most recent to the least recent
            if (cur == NULL)
            {
                printf("empty\n");
                return;
            }
            while (cur != NULL)
            {
                printf("%lu->", (unsigned long)cur->obj_id);
                cur = cur->queue.next;
            }
            printf("END\n");
        }

        std::vector<uint64_t> insert(obj_id_t id, int64_t size) // 插入新对象
        {
            std::vector<uint64_t> evicted;
            if ((uint64_t)size > cache_size) // 不准入
            {
                WARN("Object size exceeds cache capacity, object size: {}, cache capacity: {}", size, cache_size);
                cache_algo_stats["numEvictions"]++;
                cache_algo_stats["sizeEvictions"] += size;
                evicted.push_back(id);
                return evicted;
            }

            while (current_size + size > cache_size) // 驱逐直到空间足够容纳当前对象
            {
                cache_obj_t *evict_id = lru_tail; // 驱逐LRU尾部对象
                cache_algo_stats["numEvictions"]++;
                cache_algo_stats["sizeEvictions"] += evict_id->obj_size;
                evicted.push_back(evict_id->obj_id);
                remove(evict_id->obj_id);
            }
            cache_obj_t *new_entry = tags.allocate(id, size);     // 为对象创建一个新链表节点
            prepend_obj_to_head(&lru_head, &lru_tail, new_entry); // 插入头部
            current_size += size;
            cache_algo_stats["current_size"] = current_size;
            assert(current_size <= cache_size);
            return evicted;
        }

        bool get(obj_id_t id) // 读请求一个对象
        {
            auto *entry = tags.lookup(id);
            if ((bool)entry)
            {
                move_obj_to_head(&lru_head, &lru_tail, entry);
                cache_algo_stats["hits"]++;
            }
            else
            {
                cache_algo_stats["misses"]++;
            }
            return (bool)entry;
        }

        std::vector<uint64_t> set(obj_id_t id, int64_t size) // 读请求miss后插入一个对象，或者写请求写入/更新一个对象
        {
            auto *entry = tags.lookup(id);
            if ((bool)entry) // 对象已经存在，更新(set一个已经存在的对象)
            {
                remove(id);              // 先驱逐原记录
                return insert(id, size); // 再插入新记录
            }
            else // 对象不存在，插入(get miss时或set一个新对象)
            {
                return insert(id, size);
            }
        }

    private:
        cache_obj_t *lru_head;
        cache_obj_t *lru_tail;
    };

} // namespace CacheAlgo