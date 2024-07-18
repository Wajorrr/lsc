#pragma once

#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "eviction_algo_base.h"
#include "common/logging.h"
#include "stats/stats.hpp"
#include "tags.hpp"
#include "fifo.hpp"

namespace CacheAlgo
{
    class S3FIFO : public virtual CacheAlgoAbstract
    {
    public:
        S3FIFO(stats::StatsCollector *sc, uint64_t _max_size, stats::LocalStatsCollector &_cache_algo_stats, const char *cache_specific_params = NULL)
            : CacheAlgoAbstract("S3FIFO", _max_size, _cache_algo_stats)
        {
            statsCollector = sc;
            cache_algo_stats["s3fifoCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            hit_on_ghost = false;
            S3FIFO_parse_params(DEFAULT_CACHE_PARAMS);
            if (cache_specific_params != NULL)
                S3FIFO_parse_params(cache_specific_params);

            int64_t fifo_cache_size = (int64_t)cache_size * fifo_size_ratio;
            int64_t main_cache_size = cache_size - fifo_cache_size;
            int64_t fifo_ghost_cache_size = (int64_t)(cache_size * ghost_size_ratio);

            auto &fifo_stats = cache_algo_stats.createChild("fifo");
            fifo = new FIFO(fifo_cache_size, fifo_stats);

            if (fifo_ghost_cache_size > 0)
            {
                auto &fifo_ghost_stats = cache_algo_stats.createChild("fifo_ghost");
                fifo_ghost = new FIFO(fifo_ghost_cache_size, fifo_ghost_stats);
                snprintf(fifo_ghost->cache_name, CACHE_NAME_ARRAY_LEN, "FIFO-ghost");
            }
            else
                fifo_ghost = NULL;

            auto &main_cache_stats = cache_algo_stats.createChild("main_cache");
            main_cache = new FIFO(main_cache_size, main_cache_stats);
#if defined(TRACK_EVICTION_V_AGE)
            if (fifo_ghost != NULL)
            {
                fifo_ghost->track_eviction_age = false;
            }
            fifo->track_eviction_age = false;
            main_cache->track_eviction_age = false;
#endif
            snprintf(cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFO-%.4lf-%d",
                     fifo_size_ratio, move_to_main_threshold);

            req_local = new parser::Request();
        }

        ~S3FIFO()
        {
            delete fifo;
            delete main_cache;
            if (fifo_ghost != NULL)
                delete fifo_ghost;
            delete req_local;
        }

        const char *S3FIFO_current_params()
        {
            static __thread char params_str[128];
            snprintf(params_str, 128, "fifo-size-ratio=%.4lf,main-cache=%s\n",
                     fifo_size_ratio, main_cache->cache_name);
            return params_str;
        }

        void S3FIFO_parse_params(const char *cache_specific_params)
        {
            char *params_str = strdup(cache_specific_params);
            char *old_params_str = params_str;
            // char *end;

            while (params_str != NULL && params_str[0] != '\0')
            {
                /* different parameters are separated by comma,
                 * key and value are separated by = */
                char *key = strsep((char **)&params_str, "=");
                char *value = strsep((char **)&params_str, ",");

                // skip the white space
                while (params_str != NULL && *params_str == ' ')
                {
                    params_str++;
                }

                if (strcasecmp(key, "fifo-size-ratio") == 0)
                {
                    fifo_size_ratio = strtod(value, NULL);
                }
                else if (strcasecmp(key, "ghost-size-ratio") == 0)
                {
                    ghost_size_ratio = strtod(value, NULL);
                }
                else if (strcasecmp(key, "move-to-main-threshold") == 0)
                {
                    move_to_main_threshold = atoi(value);
                }
                else if (strcasecmp(key, "print") == 0)
                {
                    printf("parameters: %s\n", S3FIFO_current_params());
                    exit(0);
                }
                else
                {
                    ERROR("%s does not have parameter %s\n", cache_name, key);
                    exit(1);
                }
            }

            free(old_params_str);
        }

        bool get(const parser::Request *req, bool set_on_miss)
        {
            DEBUG_ASSERT(fifo->get_current_size() + main_cache->get_current_size() <=
                         cache_size);
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
            // 每次操作后重置hit_on_ghost
            hit_on_ghost = false;
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

            cache_algo_stats["current_size"] = get_current_size();
            DEBUG_ASSERT(get_current_size() <= cache_size);
            // 每次操作后重置hit_on_ghost
            hit_on_ghost = false;
            return evicted;
        }

        cache_obj_t *find(const parser::Request *req, const bool update_cache)
        {
            // if update cache is false, we only check the fifo and main caches
            if (!update_cache)
            {
                cache_obj_t *obj = fifo->find(req, false);
                if (obj != NULL)
                {
                    return obj;
                }
                obj = main_cache->find(req, false);
                if (obj != NULL)
                {
                    return obj;
                }
                return NULL;
            }

            /* update cache is true from now */
            hit_on_ghost = false;
            cache_obj_t *obj = fifo->find(req, true);
            if (obj != NULL)
            {
                obj->S3FIFO.freq += 1;
                return obj;
            }

            if (fifo_ghost != NULL && fifo_ghost->remove(req->id))
            {
                // if object in fifo_ghost, remove will return true
                hit_on_ghost = true;
            }

            obj = main_cache->find(req, true);
            if (obj != NULL)
            {
                obj->S3FIFO.freq += 1;
            }

            return obj;
        }

        cache_obj_t *insert(const parser::Request *req)
        {
            cache_obj_t *obj = NULL;

            if (hit_on_ghost)
            {
                /* insert into the ARC */
                hit_on_ghost = false;
                n_obj_admit_to_main += 1;
                n_byte_admit_to_main += req->req_size;
                obj = main_cache->insert(req);
            }
            else
            {
                /* insert into the fifo */
                if (req->req_size >= fifo->cache_size)
                {
                    return NULL;
                }
                n_obj_admit_to_fifo += 1;
                n_byte_admit_to_fifo += req->req_size;
                obj = fifo->insert(req);
            }

#if defined(TRACK_EVICTION_V_AGE)
            obj->create_time = CURR_TIME(*this, req);
#endif

#if defined(TRACK_DEMOTION)
            obj->create_time = req_num;
#endif

            obj->S3FIFO.freq = 0;

            return obj;
        }

        cache_obj_t *to_evict(const parser::Request *req)
        {
            assert(false);
            return NULL;
        }

        obj_id_t S3FIFO_evict_fifo(const parser::Request *req)
        {
            // bool has_evicted = false;
            obj_id_t has_evicted = 0;
            // 可能有对象需要被继续插入到主缓存中，因此循环直至实际驱逐掉一个对象
            while (!has_evicted && fifo->get_current_size() > 0)
            {
                // evict from FIFO
                cache_obj_t *obj_to_evict = fifo->to_evict(req);
                DEBUG_ASSERT(obj_to_evict != NULL);
                // need to copy the object before it is evicted
                copy_cache_obj_to_request(req_local, obj_to_evict);

                if (obj_to_evict->S3FIFO.freq >= move_to_main_threshold)
                {
#if defined(TRACK_DEMOTION)
                    printf("%ld keep %ld %ld\n", cache->n_req, obj_to_evict->create_time,
                           obj_to_evict->misc.next_access_vtime);
#endif
                    // freq is updated in cache_find_base
                    n_obj_move_to_main += 1;
                    n_byte_move_to_main += obj_to_evict->obj_size;

                    cache_obj_t *new_obj = main_cache->insert(req_local);
                    new_obj->misc.freq = obj_to_evict->misc.freq;
#if defined(TRACK_EVICTION_V_AGE)
                    new_obj->create_time = obj_to_evict->create_time;
                }
                else
                {
                    record_eviction_age(obj_to_evict,
                                        CURR_TIME(*this, req) - obj_to_evict->create_time);
#else
                }
                else
                {
#endif

#if defined(TRACK_DEMOTION)
                    printf("%ld demote %ld %ld\n", cache->n_req, obj_to_evict->create_time,
                           obj_to_evict->misc.next_access_vtime);
#endif

                    // insert to ghost
                    if (fifo_ghost != NULL)
                    {
                        fifo_ghost->get(req_local, true);
                    }
                    // has_evicted = true;
                    has_evicted = obj_to_evict->obj_id;
                }

                // remove from fifo, but do not update stat
                bool removed = fifo->remove(req_local->id);
                assert(removed);
            }
            return has_evicted;
        }

        obj_id_t S3FIFO_evict_main(const parser::Request *req)
        {
            // evict from main cache
            obj_id_t has_evicted = 0;
            // 可能有需要被重新插入的对象，因此循环直至实际驱逐掉一个对象
            while (!has_evicted && main_cache->get_current_size() > 0)
            {
                cache_obj_t *obj_to_evict = main_cache->to_evict(req);
                DEBUG_ASSERT(obj_to_evict != NULL);
                int freq = obj_to_evict->S3FIFO.freq;
#if defined(TRACK_EVICTION_V_AGE)
                int64_t create_time = obj_to_evict->create_time;
#endif
                copy_cache_obj_to_request(req_local, obj_to_evict);
                if (freq >= 1)
                {
                    // we need to evict first because the object to insert has the same obj_id
                    // main->evict(main, req);
                    main_cache->remove(obj_to_evict->obj_id);
                    obj_to_evict = NULL;

                    cache_obj_t *new_obj = main_cache->insert(req_local);
                    // clock with 2-bit counter
                    new_obj->S3FIFO.freq = MIN(freq, 3) - 1;
                    new_obj->misc.freq = freq;

#if defined(TRACK_EVICTION_V_AGE)
                    new_obj->create_time = create_time;
#endif
                }
                else
                {
#if defined(TRACK_EVICTION_V_AGE)
                    record_eviction_age(obj_to_evict,
                                        CURR_TIME(*this, req) - obj_to_evict->create_time);
#endif

                    has_evicted = obj_to_evict->obj_id;

                    // main->evict(main, req);
                    bool removed = main_cache->remove(obj_to_evict->obj_id);
                    if (!removed)
                    {
                        ERROR("cannot remove obj %ld\n", (long)obj_to_evict->obj_id);
                    }
                }
            }
            return has_evicted;
        }

        obj_id_t evict(const parser::Request *req)
        {
            // 主缓存的大小不是严格小于其预设容量，可以超出
            // 主缓存容量超出/或试用队列容量为0时，首先从主缓存中驱逐
            if (main_cache->get_current_size() > main_cache->cache_size ||
                fifo->get_current_size() == 0)
            {
                return S3FIFO_evict_main(req);
            }
            return S3FIFO_evict_fifo(req);
        }

        bool remove(const obj_id_t id)
        {
            bool removed = false;
            removed = removed || fifo->remove(id);
            removed = removed || (fifo_ghost && fifo_ghost->remove(id));
            removed = removed || main_cache->remove(id);

            return removed;
        }

        int64_t get_current_size()
        {
            return fifo->get_current_size() +
                   main_cache->get_current_size();
        }
        int64_t get_obj_num()
        {
            return fifo->get_obj_num() +
                   main_cache->get_obj_num();
        }

        bool can_insert(const parser::Request *req)
        {
            return req->req_size <= fifo->cache_size;
        }

        void print_cache()
        {
            printf("FIFO: ");
            fifo->print_cache();
            printf("Main cache: ");
            main_cache->print_cache();
            printf("FIFO-ghost: ");
            if (fifo_ghost != NULL)
            {
                fifo_ghost->print_cache();
            }
            else
            {
                printf("ghost queue is null!\n");
            }
        }

    private:
        stats::StatsCollector *statsCollector = nullptr;

        FIFO *fifo;
        FIFO *main_cache;
        FIFO *fifo_ghost;
        bool hit_on_ghost;

        int64_t n_obj_admit_to_fifo;
        int64_t n_obj_admit_to_main;
        int64_t n_obj_move_to_main;
        int64_t n_byte_admit_to_fifo;
        int64_t n_byte_admit_to_main;
        int64_t n_byte_move_to_main;

        int move_to_main_threshold;
        double fifo_size_ratio;
        double ghost_size_ratio;
        char main_cache_type[32];

        parser::Request *req_local;

        const char *DEFAULT_CACHE_PARAMS =
            "fifo-size-ratio=0.10,ghost-size-ratio=0.90,move-to-main-threshold=2";
    };

} // namespace CacheAlgo