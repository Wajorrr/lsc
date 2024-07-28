#pragma once

#include <vector>
#include "cacheAlgo/cache_obj.h"
#include "cacheAlgo/tags.hpp"
#include "parsers/parser.hpp"
#include "common/logging.h"
#include "common/macro.h"

namespace CacheAlgo
{
// #define EVICTION_AGE_ARRAY_SZE 40
#define EVICTION_AGE_ARRAY_SZE 320
#define EVICTION_AGE_LOG_BASE 1.08
#define CACHE_NAME_ARRAY_LEN 64
#define CACHE_INIT_PARAMS_LEN 256
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

        virtual ~CacheAlgoAbstract()
        {
            tags.free_hashtable();
        };

        // virtual cache_obj_t evict(cache_obj_t item) = 0;

        // find(命中时提升)+set_on_miss为true时缺失时insert
        virtual bool get(const parser::Request *req, bool set_on_miss) = 0;
        // find，若update_cache为true，则进行提升，否则只是find
        virtual cache_obj_t *find(const parser::Request *req, const bool update_cache) = 0;
        // 写入，若对象存在则更新(先删除再插入/原地更新)，否则直接插入，插入时容量不够会进行驱逐
        virtual std::vector<obj_id_t> set(const parser::Request *req, bool reinsert_on_update) = 0;
        // 只插入对象，默认容量足够
        virtual cache_obj_t *insert(const parser::Request *req) = 0;
        // virtual bool need_evict(const parser::Request *req) = 0;
        // 返回要驱逐的候选项
        virtual cache_obj_t *to_evict(const parser::Request *req) = 0;
        // 根据驱逐算法对候选项进行驱逐(对象是候选项)
        virtual obj_id_t evict(const parser::Request *req) = 0;
        // 删除指定项(对象是任意指定项)
        virtual bool remove(const obj_id_t id) = 0;

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

        void update_obj(const parser::Request *req)
        {
            cache_obj_t *obj = find(req, false);
            if (obj == NULL)
            {
                return;
            }
            current_size += (int64_t)req->req_size - obj->obj_size;
            obj->obj_size = req->req_size;
            obj->misc.next_access_vtime = req->next_access_vtime;
        }

        virtual void print_cache() = 0;
        virtual int64_t get_current_size()
        {
            return current_size;
        }
        virtual int64_t get_obj_num()
        {
            return obj_num;
        }
        int64_t get_logical_time()
        {
            return req_num;
        }
        void print_cache_stat()
        {
            printf(
                "%s cache size %ld, occupied size %ld, n_req %ld, n_obj %ld, default TTL "
                "%ld, per_obj_metadata_size %d\n",
                cache_name, (long)cache_size,
                (long)get_current_size(), (long)req_num,
                (long)get_obj_num(), (long)default_ttl,
                (int)obj_md_size);
        }
        void record_log2_eviction_age(const unsigned long long age)
        { // 更新对象驱逐年龄统计信息
            int age_log2 = age == 0 ? 0 : LOG2_ULL(age);
            log_eviction_age_cnt[age_log2] += 1;
        }

        void record_eviction_age(cache_obj_t *obj, const int64_t age)
        {
#if defined(TRACK_EVICTION_V_AGE)
            // note that the frequency is not correct for QDLP and Clock
            if (obj->obj_id % 101 == 0)
            {
                printf("%ld: %lu %ld %d\n", req_num, obj->obj_id, age, obj->misc.freq);
            }
#endif

            double log_base = log(EVICTION_AGE_LOG_BASE);
            int age_log = age == 0 ? 0 : (int)ceil(log((double)age) / log_base);
            log_eviction_age_cnt[age_log] += 1;
        }

        void print_log2_eviction_age()
        {
            printf("eviction age %d:%ld, ", 1, (long)log_eviction_age_cnt[0]);
            for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                if (log_eviction_age_cnt[i] > 1000000)
                    printf("%lu:%.1lfm, ", 1lu << i, (double)log_eviction_age_cnt[i] / 1000000.0);
                else if (log_eviction_age_cnt[i] > 1000)
                    printf("%lu:%.1lfk, ", 1lu << i, (double)log_eviction_age_cnt[i] / 1000.0);
                else if (log_eviction_age_cnt[i] > 0)
                    printf("%lu:%ld, ", 1lu << i, (long)log_eviction_age_cnt[i]);
            }
            printf("\n");
        }

        void print_eviction_age()
        {
            printf("eviction age %d:%ld, ", 1, (long)log_eviction_age_cnt[0]);
            for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                if (log_eviction_age_cnt[i] > 1000000)
                    printf("%lld:%.1lfm, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)),
                           (double)log_eviction_age_cnt[i] / 1000000.0);
                else if (log_eviction_age_cnt[i] > 1000)
                    printf("%lld:%.1lfk, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)),
                           (double)log_eviction_age_cnt[i] / 1000.0);
                else if (log_eviction_age_cnt[i] > 0)
                    printf("%lld:%ld, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)), (long)log_eviction_age_cnt[i]);
            }
            printf("\n");
        }

        bool dump_log2_eviction_age(const char *ofilepath)
        {
            FILE *ofile = fopen(ofilepath, "a");
            if (ofile == NULL)
            {
                perror("fopen failed");
                return false;
            }

            fprintf(ofile, "%s, cache size: %lu, ", cache_name, (unsigned long)cache_size);
            fprintf(ofile, "%d:%ld, ", 1, (long)log_eviction_age_cnt[0]);
            for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                if (log_eviction_age_cnt[i] == 0)
                {
                    continue;
                }
                fprintf(ofile, "%lu:%ld, ", 1lu << i, (long)log_eviction_age_cnt[i]);
            }
            fprintf(ofile, "\n\n");

            fclose(ofile);
            return true;
        }

        bool dump_eviction_age(const char *ofilepath)
        {
            FILE *ofile = fopen(ofilepath, "a");
            if (ofile == NULL)
            {
                perror("fopen failed");
                return false;
            }

            /* dump the objects' ages at eviction */
            fprintf(ofile, "%s, eviction age, cache size: %lu, ", cache_name, (unsigned long)cache_size);
            for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                if (log_eviction_age_cnt[i] == 0)
                {
                    continue;
                }
                fprintf(ofile, "%lld:%ld, ", (long long)pow(EVICTION_AGE_LOG_BASE, i), (long)log_eviction_age_cnt[i]);
            }
            fprintf(ofile, "\n");

            fclose(ofile);
            return true;
        }

        bool dump_cached_obj_age(const parser::Request *req, const char *ofilepath)
        {
            FILE *ofile = fopen(ofilepath, "a");
            if (ofile == NULL)
            {
                perror("fopen failed");
                return false;
            }

            /* clear/reset eviction age counters */
            for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                log_eviction_age_cnt[i] = 0;
            }

            int64_t n_cached_obj = get_obj_num();
            int64_t n_evicted_obj = 0;
            /* evict all the objects */
            while (get_current_size() > 0)
            {
                evict(req);
                n_evicted_obj++;
            }
            assert(n_cached_obj == n_evicted_obj);

            int64_t n_ages = 0;
            /* dump the cached objects' ages */
            fprintf(ofile, "%s, cached_obj age, cache size: %lu, ", cache_name, (unsigned long)cache_size);
            for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++)
            {
                if (log_eviction_age_cnt[i] == 0)
                {
                    continue;
                }
                n_ages += log_eviction_age_cnt[i];
                fprintf(ofile, "%lld:%ld, ", (long long)pow(EVICTION_AGE_LOG_BASE, i), (long)log_eviction_age_cnt[i]);
            }
            fprintf(ofile, "\n");
            assert(n_ages == n_cached_obj);

            fclose(ofile);
            return true;
        }

        void record_current_size()
        {
            cache_algo_stats["current_size"] = get_current_size();
            // DEBUG_ASSERT(current_size <= cache_size);
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
            VERBOSE("******* %s get: req %ld, obj %ld, obj_size %ld, cache size %ld/%ld\n",
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

        std::vector<obj_id_t> cache_set_base(const parser::Request *req, bool reinsert_on_update)
        {
            VERBOSE("******* %s set: req %ld, obj %ld, obj_size %ld, cache size %ld/%ld\n",
                    cache_name, req_num, req->id, req->req_size,
                    get_current_size(), cache_size);

            cache_obj_t *obj = find(req, false);
            uint32_t old_size = obj ? obj->obj_size : 0;

            std::vector<obj_id_t> evicted;
            while (get_current_size() + req->req_size - old_size >
                   cache_size)
            {
                evicted.push_back(evict(req));
            }

            if (obj)
            {
                if (!reinsert_on_update)
                {
                    update_obj(req);
                    return evicted;
                }
                else
                    remove(obj->obj_id);
            }
            insert(req);

            return evicted;
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
            cache_obj->create_time = CURR_TIME(*this, req);
#endif

            cache_obj->misc.next_access_vtime = req->next_access_vtime;
            cache_obj->misc.freq = 0;

            return cache_obj;
        }

        void cache_evict_base(cache_obj_t *obj, bool remove_from_hashtable)
        {
#if defined(TRACK_EVICTION_V_AGE)
            if (track_eviction_age)
            {
                record_eviction_age(obj, req_num - obj->create_time);
            }
#endif

            cache_remove_obj_base(obj, remove_from_hashtable);
        }

        void cache_remove_obj_base(cache_obj_t *obj, bool remove_from_hashtable)
        {
            DEBUG_ASSERT(current_size >= obj->obj_size + obj_md_size);
            current_size -= (obj->obj_size + obj_md_size);
            obj_num -= 1;
            cache_algo_stats["numEvictions"]++;
            cache_algo_stats["sizeEvictions"] += obj->obj_size;

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

        char cache_name[CACHE_NAME_ARRAY_LEN];

        int64_t to_evict_candidate_gen_vtime;

#if defined(TRACK_EVICTION_V_AGE)
        bool track_eviction_age;
#endif
#if defined(TRACK_DEMOTION)
        bool track_demotion;
#endif

        int64_t log_eviction_age_cnt[EVICTION_AGE_ARRAY_SZE]; // 对象驱逐年龄统计信息

    protected:
        int64_t obj_num;
        int64_t current_size;

        stats::LocalStatsCollector &cache_algo_stats;
    };
}