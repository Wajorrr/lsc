#pragma once

#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "eviction_algo_base.h"
#include "common/logging.h"
#include "tags.hpp"

namespace CacheAlgo
{
    class SIEVE : public virtual CacheAlgoAbstract
    {
    public:
        SIEVE(uint64_t _max_size, stats::LocalStatsCollector &_cache_algo_stats)
            : CacheAlgoAbstract("Sieve", _max_size, _cache_algo_stats)
        {
            cache_algo_stats["SieveCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            q_head = nullptr;
            q_tail = nullptr;
            pointer = nullptr;

            // if (consider_obj_metadata)
            // {
            //     obj_md_size = 1;
            // }
            // else
            // {
            //     obj_md_size = 0;
            // }
        }

        bool get(const parser::Request *req, bool set_on_miss)
        {
            req_num += 1;
            bool hit = false;
            if (set_on_miss)
                hit = cache_get_base(req);
            else
                hit = (bool)find(req, true);
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
            cache_obj_t *cache_obj = cache_find_base(req, update_cache);
            if (cache_obj != NULL && update_cache)
            {
                cache_obj->sieve.freq = 1;
            }
            return cache_obj;
        }

        cache_obj_t *insert(const parser::Request *req)
        {
            cache_obj_t *obj = cache_insert_base(req);
            prepend_obj_to_head(&q_head, &q_tail, obj);
            obj->sieve.freq = 0;

            return obj;
        }

        cache_obj_t *Sieve_to_evict_with_freq(const parser::Request *req,
                                              int to_evict_freq)
        {
            cache_obj_t *pointer = pointer;

            /* if we have run one full around or first eviction */
            if (pointer == NULL)
                pointer = q_tail;

            /* find the first untouched */
            while (pointer != NULL && pointer->sieve.freq > to_evict_freq)
            {
                pointer = pointer->queue.prev;
            }

            /* if we have finished one around, start from the tail */
            if (pointer == NULL)
            {
                pointer = q_tail;
                while (pointer != NULL && pointer->sieve.freq > to_evict_freq)
                {
                    pointer = pointer->queue.prev;
                }
            }

            if (pointer == NULL)
                return NULL;

            return pointer;
        }

        cache_obj_t *to_evict(const parser::Request *req)
        {
            // because we do not change the frequency of the object,
            // if all objects have frequency 1, we may return NULL
            int to_evict_freq = 0;

            cache_obj_t *obj_to_evict =
                Sieve_to_evict_with_freq(req, to_evict_freq);

            while (obj_to_evict == NULL)
            {
                to_evict_freq += 1;

                obj_to_evict = Sieve_to_evict_with_freq(req, to_evict_freq);
            }

            return obj_to_evict;
        }

        obj_id_t evict(const parser::Request *req)
        {
            /* if we have run one full around or first eviction */
            cache_obj_t *obj = pointer == NULL ? q_tail : pointer;

            while (obj->sieve.freq > 0)
            {
                obj->sieve.freq -= 1;
                obj = obj->queue.prev == NULL ? q_tail : obj->queue.prev;
            }

            pointer = obj->queue.prev;
            remove_obj_from_list(&q_head, &q_tail, obj);

            obj_id_t evicted = obj->obj_id;
            cache_evict_base(obj, true);
            return evicted;
        }

        void Sieve_remove_obj(cache_obj_t *obj_to_remove)
        {
            DEBUG_ASSERT(obj_to_remove != NULL);
            if (obj_to_remove == pointer)
            {
                pointer = obj_to_remove->queue.prev;
            }
            remove_obj_from_list(&q_head, &q_tail, obj_to_remove);
            cache_remove_obj_base(obj_to_remove, true);
        }

        bool remove(const obj_id_t id)
        {
            cache_obj_t *obj = tags.hashtable_find_obj_id(id);
            if (obj == NULL)
            {
                return false;
            }

            Sieve_remove_obj(obj);

            return true;
        }

        void Sieve_verify()
        {
            int64_t n_obj = 0, n_byte = 0;
            cache_obj_t *obj = q_head;

            while (obj != NULL)
            {
                assert(tags.hashtable_find_obj_id(obj->obj_id) != NULL);
                n_obj++;
                n_byte += obj->obj_size;
                obj = obj->queue.next;
            }

            assert(n_obj == get_obj_num());
            assert(n_byte == get_current_size());
        }

        void print_cache()
        {
            cache_obj_t *cur = q_head;
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
        cache_obj_t *q_head;
        cache_obj_t *q_tail;
        cache_obj_t *pointer;
    };

} // namespace CacheAlgo