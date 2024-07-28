#pragma once

#include "stats/stats.hpp"
#include "cache_algo_abstract.hpp"
#include "block.hpp"
#include "eviction_algo_base.h"
#include "common/logging.h"
#include "stats/stats.hpp"
#include "tags.hpp"
#include "fifo.hpp"
#include "lru.hpp"
#include "sieve.hpp"

namespace CacheAlgo
{
    class S3FIFOd : public virtual CacheAlgoAbstract
    {
    public:
        S3FIFOd(stats::StatsCollector *sc, uint64_t _max_size, stats::LocalStatsCollector &_cache_algo_stats, const char *cache_specific_params = NULL)
            : CacheAlgoAbstract("S3FIFOd", _max_size, _cache_algo_stats)
        {
            statsCollector = sc;
            cache_algo_stats["s3fifodCacheCapacity"] = _max_size; // 缓存容量，单位为字节
            hit_on_ghost = false;
            S3FIFO_parse_params(DEFAULT_CACHE_PARAMS);
            if (cache_specific_params != NULL)
                S3FIFO_parse_params(cache_specific_params);

            int64_t fifo_cache_size = (int64_t)cache_size * fifo_size_ratio;
            int64_t main_cache_size = cache_size - fifo_cache_size;
            // int64_t fifo_ghost_cache_size = (int64_t)(cache_size * ghost_size_ratio);
            int64_t fifo_ghost_cache_size = main_cache_size;

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

            if (strcasecmp(main_cache_type, "FIFO") == 0)
            {
                main_cache = new FIFO(main_cache_size, main_cache_stats);
            }
            else if (strcasecmp(main_cache_type, "LRU") == 0)
            {
                main_cache = new LRU(main_cache_size, main_cache_stats);
            }
            else if (strcasecmp(main_cache_type, "Sieve") == 0)
            {
                main_cache = new SIEVE(main_cache_size, main_cache_stats);
            }
            else
            {
                ERROR("S3FIFOd does not support %s \n", main_cache_type);
            }

            int64_t eviction_size = cache_size / 10;
            auto &fifo_eviction_stats = cache_algo_stats.createChild("fifo_eviction");
            fifo_eviction = new FIFO(eviction_size, fifo_eviction_stats);

            auto &main_cache_eviction_stats = cache_algo_stats.createChild("main_cache_eviction");
            main_cache_eviction = new FIFO(eviction_size, main_cache_eviction_stats);

            snprintf(fifo_eviction->cache_name, CACHE_NAME_ARRAY_LEN,
                     "FIFO-evicted");
            snprintf(main_cache_eviction->cache_name, CACHE_NAME_ARRAY_LEN, "%s",
                     "main-evicted");

#if defined(TRACK_EVICTION_V_AGE)
            fifo->track_eviction_age = false;
            main_cache->track_eviction_age = false;
            fifo_ghost->track_eviction_age = false;
            fifo_eviction->track_eviction_age = false;
            main_cache_eviction->track_eviction_age = false;
#endif

            // snprintf(cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFO-%.4lf-%d",
            //          fifo_size_ratio, move_to_main_threshold);
            snprintf(cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFOd-%s-%d",
                     main_cache_type, move_to_main_threshold);

            req_local = new parser::Request();
        }

        ~S3FIFOd()
        {
            delete fifo;
            delete main_cache;
            if (fifo_ghost != NULL)
                delete fifo_ghost;
            delete req_local;
            delete fifo_eviction;
            delete main_cache_eviction;
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
                else if (strcasecmp(key, "main-cache") == 0)
                {
                    strncpy(main_cache_type, value, 30);
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

        void S3FIFOd_update_fifo_size(const parser::Request *req)
        {
            int step = 20; // 容量调整大小
            step = MAX(1, MIN(fifo->cache_size, main_cache->cache_size) / 1000);
            bool cond1 = fifo_eviction_hit + main_eviction_hit > 100;
            bool cond2 = main_cache_eviction->get_current_size() > 0;

            // 若主缓存驱逐队列为空，重置驱逐队列命中计数
            if (!cond2)
            {
                fifo_eviction_hit = 0;
                main_eviction_hit = 0;
            }

            // 若主缓存驱逐队列不为空，且small fifo和main fifo的驱逐队列命中次数总和大于100
            // 则进行容量调整
            if (cond1 && cond2)
            {
                // 若一个驱逐队列的命中次数大于另一个驱逐队列命中次数的两倍
                // 且另一个缓存的容量足够大，则调整容量
                if (fifo_eviction_hit > main_eviction_hit * 2)
                {
                    // if (main_cache->cache_size > step) {
                    if (main_cache->cache_size > cache_size / 100)
                    {
                        fifo->cache_size += step;
                        fifo_ghost->cache_size += step;
                        main_cache->cache_size -= step;
                    }
                }
                else if (main_eviction_hit > fifo_eviction_hit * 2)
                {
                    // if (fifo->cache_size > step) {
                    if (fifo->cache_size > cache_size / 100)
                    {
                        fifo->cache_size -= step;
                        fifo_ghost->cache_size -= step;
                        main_cache->cache_size += step;
                    }
                }
                // 调整驱逐队列的命中计数
                fifo_eviction_hit = fifo_eviction_hit * 0.8;
                main_eviction_hit = main_eviction_hit * 0.8;
            }
        }

        void S3FIFOd_update_fifo_size2(const parser::Request *req)
        {
            assert(fifo_eviction_hit + main_eviction_hit <= 1);
            if (fifo_eviction_hit == 1 && main_cache->cache_size > 1)
            {
                fifo->cache_size += 1;
                main_cache->cache_size -= 1;
            }
            else if (main_eviction_hit == 1 && fifo->cache_size > 1)
            {
                main_cache->cache_size += 1;
                fifo->cache_size -= 1;
            }
            fifo_eviction_hit = 0;
            main_eviction_hit = 0;
        }

        bool get(const parser::Request *req, bool set_on_miss)
        {
            DEBUG_ASSERT(fifo->get_current_size() + main_cache->get_current_size() <=
                         cache_size);
            req_num += 1;
            bool hit = false;
            if (set_on_miss)
            {
                S3FIFOd_update_fifo_size(req);
                hit = cache_get_base(req);
            }
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
            S3FIFOd_update_fifo_size(req);

            std::vector<obj_id_t> evicted;
            if (!can_insert(req)) // 不准入
            {
                cache_algo_stats["numEvictions"]++;
                cache_algo_stats["sizeEvictions"] += req->req_size;
                evicted.push_back(req->id);
                return evicted;
            }

            { // 驱逐并进行更新/写入
                cache_obj_t *obj = find(req, false);
                uint32_t old_size = obj ? obj->obj_size : 0;

                while (get_current_size() + req->req_size - old_size >
                       cache_size)
                {
                    std::vector<obj_id_t> temp = evicts(req);
                    evicted.insert(evicted.end(), temp.begin(), temp.end());
                }

                if (obj) // 更新
                {
                    if (!reinsert_on_update)
                    {
                        update_obj(req);
                    }
                    else
                        remove(obj->obj_id);
                }
                if (reinsert_on_update || !obj)
                    insert(req);
            }

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
                // obj->S3FIFO.freq += 1;
                return obj;
            }

            if (fifo_ghost->remove(req->id))
            {
                // if object in fifo_ghost, remove will return true
                hit_on_ghost = true;
            }

            obj = main_cache->find(req, true);
            // if (obj != NULL)
            // {
            //     obj->S3FIFO.freq += 1;
            // }

            // 在驱逐队列中命中了
            if (fifo_eviction->find(req, false) != NULL)
            {
                fifo_eviction->remove(req->id);
                fifo_eviction_hit++;
            }

            if (main_cache_eviction->find(req, true) != NULL)
            {
                main_cache_eviction->remove(req->id);
                main_eviction_hit++;
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
                // main cache的容量限制是严格的，因此插入前需要先驱逐
                main_cache->get(req, true);
                obj = main_cache->find(req, false);
            }
            else
            {
                /* insert into the fifo */
                if (req->req_size >= fifo->cache_size)
                {
                    return NULL;
                }
                obj = fifo->insert(req);
            }
            assert(obj->misc.freq == 0);

#if defined(TRACK_EVICTION_V_AGE)
            obj->create_time = CURR_TIME(*this, req);
#endif

#if defined(TRACK_DEMOTION)
            obj->create_time = req_num;
#endif

            // obj->S3FIFO.freq = 0;

            return obj;
        }

        cache_obj_t *to_evict(const parser::Request *req)
        {
            assert(false);
            return NULL;
        }

        obj_id_t evict(const parser::Request *req)
        {
            obj_id_t evict_obj_id = 0;
            // small fifo队列为空，从main fifo中驱逐
            if (fifo->get_current_size() == 0)
            {
                assert(main_cache->get_current_size() <= cache_size);
                // evict from main cache
                cache_obj_t *obj = main_cache->to_evict(req);
#if defined(TRACK_EVICTION_V_AGE)
                record_eviction_age(obj, CURR_TIME(*this, req) - obj->create_time);
#endif
                copy_cache_obj_to_request(req_local, obj);
                // main中驱逐出的对象，插入到驱逐队列
                main_cache_eviction->get(req_local, true);
                evict_obj_id = main_cache->evict(req);
                return evict_obj_id;
            }

            // small FIFO队列不为空，从small FIFO队列中驱逐
            cache_obj_t *obj = fifo->to_evict(req);
            assert(obj != NULL);
            // need to copy the object before it is evicted
            copy_cache_obj_to_request(req_local, obj);

#if defined(TRACK_EVICTION_V_AGE)
            if (obj->misc.freq >= move_to_main_threshold)
            {
                // promote to main cache
                cache_obj_t *new_obj = main_cache->insert(req_local);
                new_obj->create_time = obj->create_time;
                // evict from fifo, must be after copy eviction age
                bool removed = fifo->remove(req_local->id);
                assert(removed);

                while (main_cache->get_current_size() > main_cache->cache_size)
                {
                    // evict from main cache
                    obj = main_cache->to_evict(req);
                    copy_cache_obj_to_request(req_local, obj);
                    main_cache_eviction->get(req_local, true);
                    evict_obj_id = main_cache->evict(req);
                }
            }
            else
            {
                // evict from fifo, must be after copy eviction age
                bool removed = fifo->remove(req_local->id);
                assert(removed);
                evict_obj_id = req_local->id;

                record_eviction_age(obj, CURR_TIME(*this, req) - obj->create_time);
                // insert to ghost
                fifo_ghost->get(req_local, true);
                fifo_eviction->get(req_local, true);
            }
            return evict_obj_id;

#else
            // small FIFO队列不为空，从small FIFO队列中驱逐
            bool removed = fifo->remove(req_local->id);
            assert(removed);

            // 将驱逐的对象重插入到main fifo中
            if (obj->misc.freq >= move_to_main_threshold)
            {
                // promote to main cache
                main_cache->insert(req_local);

                // 若主缓存容量超限，驱逐直到容量满足
                while (main_cache->get_current_size() > main_cache->cache_size)
                {
                    // evict from main cache
                    obj = main_cache->to_evict(req);
                    copy_cache_obj_to_request(req_local, obj);
                    // 被驱逐的对象插入到驱逐队列
                    main_cache_eviction->get(req_local, true);
                    evict_obj_id = main_cache->evict(req);
                }
            }
            else // 从small fifo队列中直接驱逐的对象插入到驱逐队列
            {
                evict_obj_id = req_local->id;
                // insert to ghost
                fifo_ghost->get(req_local, true);
                fifo_eviction->get(req_local, true);
            }
            return evict_obj_id;
#endif
        }

        std::vector<obj_id_t> evicts(const parser::Request *req)
        {
            std::vector<obj_id_t> evicted;
            // small fifo队列为空，从main fifo中驱逐
            if (fifo->get_current_size() == 0)
            {
                assert(main_cache->get_current_size() <= cache_size);
                // evict from main cache
                cache_obj_t *obj = main_cache->to_evict(req);
#if defined(TRACK_EVICTION_V_AGE)
                record_eviction_age(obj, CURR_TIME(*this, req) - obj->create_time);
#endif
                copy_cache_obj_to_request(req_local, obj);
                // main中驱逐出的对象，插入到驱逐队列
                main_cache_eviction->get(req_local, true);
                evicted.push_back(main_cache->evict(req));
                return evicted;
            }

            // small FIFO队列不为空，从small FIFO队列中驱逐
            cache_obj_t *obj = fifo->to_evict(req);
            assert(obj != NULL);
            // need to copy the object before it is evicted
            copy_cache_obj_to_request(req_local, obj);

#if defined(TRACK_EVICTION_V_AGE)
            if (obj->misc.freq >= move_to_main_threshold)
            {
                // promote to main cache
                cache_obj_t *new_obj = main_cache->insert(req_local);
                new_obj->create_time = obj->create_time;
                // evict from fifo, must be after copy eviction age
                bool removed = fifo->remove(req_local->id);
                assert(removed);

                while (main_cache->get_current_size() > main_cache->cache_size)
                {
                    // evict from main cache
                    obj = main_cache->to_evict(req);
                    copy_cache_obj_to_request(req_local, obj);
                    main_cache_eviction->get(req_local, true);
                    evicted.push_back(main_cache->evict(req));
                }
            }
            else
            {
                // evict from fifo, must be after copy eviction age
                bool removed = fifo->remove(req_local->id);
                assert(removed);
                evicted.push_back(req_local->id);

                record_eviction_age(obj, CURR_TIME(*this, req) - obj->create_time);
                // insert to ghost
                fifo_ghost->get(req_local, true);
                fifo_eviction->get(req_local, true);
            }
            return evicted;

#else
            // small FIFO队列不为空，从small FIFO队列中驱逐
            bool removed = fifo->remove(req_local->id);
            assert(removed);

            // 将驱逐的对象重插入到main fifo中
            if (obj->misc.freq >= move_to_main_threshold)
            {
                // promote to main cache
                main_cache->insert(req_local);

                // 若主缓存容量超限，驱逐直到容量满足
                while (main_cache->get_current_size() > main_cache->cache_size)
                {
                    // evict from main cache
                    obj = main_cache->to_evict(req);
                    copy_cache_obj_to_request(req_local, obj);
                    // 被驱逐的对象插入到驱逐队列
                    main_cache_eviction->get(req_local, true);
                    evicted.push_back(main_cache->evict(req));
                }
            }
            else // 从small fifo队列中直接驱逐的对象插入到驱逐队列
            {
                evicted.push_back(req_local->id);
                // insert to ghost
                fifo_ghost->get(req_local, true);
                fifo_eviction->get(req_local, true);
            }
            return evicted;
#endif
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
        CacheAlgoAbstract *main_cache;
        FIFO *fifo_ghost;
        bool hit_on_ghost;
        int move_to_main_threshold;

        double fifo_size_ratio;
        char main_cache_type[32];

        CacheAlgoAbstract *fifo_eviction;
        CacheAlgoAbstract *main_cache_eviction;

        int32_t fifo_eviction_hit;
        int32_t main_eviction_hit;

        parser::Request *req_local;

        const char *DEFAULT_CACHE_PARAMS =
            "fifo-size-ratio=0.10,main-cache=FIFO,move-to-main-threshold=1";
    };

} // namespace CacheAlgo