#pragma once

#include <vector>
#include "cacheAlgo/cache_obj.h"
#include "cacheAlgo/tags.hpp"
#include "parsers/parser.hpp"
#include "common/logging.h"
#include "common/macro.h"

namespace CacheAlgo
{
    class CacheAlgoAbstract
    {
    public:
        CacheAlgoAbstract(const std::string &_cache_name, uint64_t _max_size, stats::LocalStatsCollector &_cache_algo_stats)
            : cache_size(_max_size), cache_algo_stats(_cache_algo_stats)
        {
            strcpy(cache_name, _cache_name.c_str());
            current_size = 0;
            obj_md_size = 0;
            req_num = 0;
            obj_num = 0;
        }

        virtual ~CacheAlgoAbstract() = default;

        virtual bool get(obj_id_t id) = 0;

        virtual std::vector<uint64_t> set(obj_id_t id, int64_t size) = 0;

        // virtual cache_obj_t evict(cache_obj_t item) = 0;

        virtual bool get(const parser::Request *req) = 0;
        virtual cache_obj_t *find(const parser::Request *req, const bool update_cache) = 0;
        virtual cache_obj_t *insert(const parser::Request *req) = 0;
        // virtual bool need_evict(const parser::Request *req) = 0;
        virtual cache_obj_t *to_evict(const parser::Request *req) = 0;
        virtual void evict(const parser::Request *req) = 0;
        virtual bool remove(const obj_id_t id) = 0;
        virtual void print_cache() = 0;
        virtual int64_t get_current_size()
        {
            return current_size;
        }
        virtual int64_t get_obj_num()
        {
            return obj_num;
        }

        virtual bool can_insert(const parser::Request *req)
        {
            if (req->req_size + obj_md_size > cache_size)
            {
                WARN_ONCE("%ld req, obj %lu, size %lu larger than cache size %lu\n",
                          (long)req_num, (unsigned long)req->id,
                          (unsigned long)req->req_size, (unsigned long)cache_size);
                return false;
            }

            return true;
        }

    protected:
        cache_obj_t *cache_find_base(const parser::Request *req, const bool update_cache)
        {
            cache_obj_t *cache_obj = tags.hashtable_find(req);

            if (cache_obj != NULL)
            {
#ifdef SUPPORT_TTL
                if (cache_obj->exp_time != 0 && cache_obj->exp_time < req->time)
                {
                    if (update_cache)
                    {
                        remove(cache_obj->obj_id);
                    }

                    cache_obj = NULL;
                }
#endif

                if (update_cache)
                {
                    cache_obj->misc.next_access_vtime = req->next_access_vtime;
                    cache_obj->misc.freq += 1;
                }
            }

            return cache_obj;
        }

        bool cache_get_base(const parser::Request *req)
        {
            req_num += 1;

            VERBOSE("******* %s req %ld, obj %ld, obj_size %ld, cache size %ld/%ld\n",
                    cache_name, req_num, req->id, req->req_size,
                    get_current_size(), cache_size);

            cache_obj_t *obj = find(req, true);
            bool hit = (obj != NULL);

            if (hit)
            {
                VVERBOSE("req %ld, obj %ld --- cache hit\n", req_num, req->id);
            }
            else if (!can_insert(req))
            {
                VVERBOSE("req %ld, obj %ld --- cache miss cannot insert\n", req_num,
                         req->id);
            }
            else
            {
                while (get_current_size() + req->req_size + obj_md_size >
                       cache_size)
                {
                    evict(req);
                }
                insert(req);
            }

            return hit;
        }

        cache_obj_t *cache_insert_base(const parser::Request *req)
        {
            cache_obj_t *cache_obj = tags.hashtable_insert(req);
            current_size +=
                (int64_t)cache_obj->obj_size + (int64_t)obj_md_size;
            obj_num += 1;

#ifdef SUPPORT_TTL
            if (default_ttl != 0 && req->ttl == 0)
            {
                cache_obj->exp_time = (int32_t)default_ttl + req->time;
            }
#endif

#if defined(TRACK_EVICTION_V_AGE) || defined(TRACK_DEMOTION) || \
    defined(TRACK_CREATE_TIME)
            cache_obj->create_time = req_num;
#endif

            cache_obj->misc.next_access_vtime = req->next_access_vtime;
            cache_obj->misc.freq = 0;

            return cache_obj;
        }

        void cache_evict_base(cache_obj_t *obj, bool remove_from_hashtable)
        {
#if defined(TRACK_EVICTION_V_AGE)
            // if (track_eviction_age)
            // {
            //     record_eviction_age(cache, obj, req_num - obj->create_time);
            // }
#endif

            cache_remove_obj_base(obj, remove_from_hashtable);
        }

        void cache_remove_obj_base(cache_obj_t *obj, bool remove_from_hashtable)
        {
            DEBUG_ASSERT(current_size >= obj->obj_size + obj_md_size);
            current_size -= (obj->obj_size + obj_md_size);
            obj_num -= 1;

            if (remove_from_hashtable)
            {
                tags.hashtable_delete(obj);
            }
        }

    public:
        Tags tags; // 一个hash表
        // const
        int64_t cache_size;
        int64_t default_ttl;
        int32_t obj_md_size; // 对象元数据大小，不同算法为每个对象维护的元数据大小不同
        int64_t req_num;     // logical_time, virtual_time, reference_count

        char cache_name[64];

        int64_t to_evict_candidate_gen_vtime;

#if defined(TRACK_EVICTION_V_AGE)
        bool track_eviction_age;
#endif
#if defined(TRACK_DEMOTION)
        bool track_demotion;
#endif

    protected:
        int64_t obj_num;
        int64_t current_size;

        stats::LocalStatsCollector &cache_algo_stats;
    };
}