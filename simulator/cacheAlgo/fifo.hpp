#pragma once

#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "common/logging.h"
#include "tags.hpp"

namespace CacheAlgo
{
    class FIFO : public virtual CacheAlgoAbstract
    {
    public:
        FIFO(uint64_t _max_size, stats::LocalStatsCollector &_cache_algo_stats)
            : CacheAlgoAbstract("FIFO", _max_size, _cache_algo_stats)
        {
            cache_algo_stats["fifoCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            fifo_head = nullptr;
            fifo_tail = nullptr;
        }

        bool get(const parser::Request *req, bool update_stats = false, bool set_on_miss = false)
        {
            req_num += 1;
            bool hit = false;
            if (set_on_miss)
                hit = cache_get_base(req);
            else
                hit = (bool)find(req, true);
            if (update_stats)
            {
                if (hit)
                {
                    cache_algo_stats["hits"]++;
                }
                else
                {
                    cache_algo_stats["misses"]++;
                }
            }
            return hit;
        }

        std::vector<obj_id_t> set(const parser::Request *req, bool reinsert_on_update)
        {
            req_num += 1;

            std::vector<obj_id_t> evicted;
            if (!can_insert(req)) // 不准入
            {
                cache_algo_stats["numEvictions"]++;
                cache_algo_stats["sizeEvictions"] += req->req_size;
                evicted.push_back(req->id);
                return evicted;
            }

            evicted = cache_set_base(req, reinsert_on_update);

            cache_algo_stats["current_size"] = current_size;
            DEBUG_ASSERT(current_size <= cache_size);
            return evicted;
        }

        cache_obj_t *find(const parser::Request *req, const bool update_cache)
        {
            return cache_find_base(req, update_cache);
        }

        cache_obj_t *insert(const parser::Request *req)
        {
            cache_obj_t *obj = cache_insert_base(req);
            prepend_obj_to_head(&fifo_head, &fifo_tail, obj);

            return obj;
        }

        cache_obj_t *to_evict(const parser::Request *req)
        {
            DEBUG_ASSERT(fifo_tail != NULL || current_size == 0);

            to_evict_candidate_gen_vtime = req_num;
            return fifo_tail;
        }

        obj_id_t evict(const parser::Request *req)
        {
            cache_obj_t *obj_to_evict = fifo_tail;
            DEBUG_ASSERT(fifo_tail != NULL);

            // we can simply call remove_obj_from_list here, but for the best performance,
            // we chose to do it manually
            // remove_obj_from_list(&fifo_head, &fifo_tail, obj)

            fifo_tail = fifo_tail->queue.prev;
            if (likely(fifo_tail != NULL))
            {
                fifo_tail->queue.next = NULL;
            }
            else
            {
                /* obj_num has not been updated */
                DEBUG_ASSERT(obj_num == 1);
                fifo_head = NULL;
            }

#if defined(TRACK_DEMOTION)
            if (cache->track_demotion)
                printf("%ld demote %ld %ld\n", req_num, obj_to_evict->create_time,
                       obj_to_evict->misc.next_access_vtime);
#endif
            obj_id_t evict_obj_id = obj_to_evict->obj_id;
            cache_evict_base(obj_to_evict, true);
            return evict_obj_id;
        }

        bool remove(const obj_id_t id)
        {
            cache_obj_t *obj = tags.hashtable_find_obj_id(id);
            if (obj == NULL)
            {
                return false;
            }

            remove_obj_from_list(&fifo_head, &fifo_tail, obj);
            cache_remove_obj_base(obj, true);

            return true;
        }

        void print_cache()
        {
            cache_obj_t *cur = fifo_head;
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

    private:
        cache_obj_t *fifo_head;
        cache_obj_t *fifo_tail;
    };

} // namespace CacheAlgo