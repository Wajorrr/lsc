//
// Created by Juncheng Yang on 4/20/20.
//

#pragma once

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/macro.h"
#include "common/logging.h"

#include "parsers/parser.hpp"

#include "analyzer/utils/struct.h"

#include "analyzer/stats/accessPattern.h"
#include "analyzer/stats/op.h"
#include "analyzer/stats/popularity.h"
#include "analyzer/stats/popularityDecay.h"
#include "analyzer/stats/reqRate.h"
#include "analyzer/stats/reuse.h"
#include "analyzer/stats/size.h"
#include "analyzer/stats/ttl.h"
#include "analyzer/stats/lifespan.h"

/* experimental module */
#include "analyzer/experimental/createFutureReuseCCDF.h"
#include "analyzer/experimental/lifetime.h"
#include "analyzer/experimental/probAtAge.h"
#include "analyzer/experimental/scanDetector.h"
#include "analyzer/experimental/sizeChange.h"

// /* deprecated module */
// #include "dep/writeFutureReuse.h"
// #include "dep/writeReuse.h"

namespace traceAnalyzer
{
    typedef struct analysis_option
    {
        bool req_rate;       // 是否统计请求速率
        bool access_pattern; // 是否统计访问模式
        bool size;           // 是否统计大小分布
        bool reuse;          // 是否统计重用时间分布
        bool popularity;     // 是否统计对象流行度(访问频次)
        bool ttl;            // 是否统计ttl

        bool popularity_decay; // 是否统计过去若干个时间窗口内，每个时间窗口的请求对象数和新对象数

        bool lifetime; // 是否统计对象生命周期

        bool create_future_reuse_ccdf; // 是否统计重用时间分布(各个重用时间的访问次数、访问年龄总和、访问频率总和)

        bool prob_at_age; // 是否统计 <重用时间经过的时间窗口数，自创建以来经过的时间窗口数> 对应的请求次数
                          // 即不同访问年龄的对象被重用的概率

        bool size_change; // 是否统计覆写对象的大小变化分布情况

        bool lifespan;
    } analysis_option_t;

    typedef struct analysis_param
    {
        int track_n_popular;
        /* tracking the number of one-hit, ... X-hit wonders */
        int track_n_hit;
        int time_window;
        int warmup_time;
        double access_pattern_sample_ratio;
        int access_pattern_sample_ratio_inv;
    } analysis_param_t;

    static analysis_param_t default_param()
    {
        analysis_param_t param;
        param.track_n_popular = 8;
        param.track_n_hit = 8;
        param.time_window = 300;
        param.warmup_time = 86400;
        param.access_pattern_sample_ratio = 0.01;
        param.access_pattern_sample_ratio_inv = 101;

        return param;
    };

    static struct analysis_option default_option()
    {
        struct analysis_option option = {0};
        option.req_rate = false;
        option.access_pattern = false;
        option.ttl = false;
        option.size = false;
        option.reuse = false;
        option.popularity = false;
        option.popularity_decay = false;
        option.create_future_reuse_ccdf = false;
        option.prob_at_age = false;
        option.size_change = false;
        option.lifetime = false;

        return option;
    };

#define DEFAULT_PREALLOC_N_OBJ 1e8

    class TraceAnalyzer
    {
    public:
        explicit TraceAnalyzer(parser::Parser *parser, string output_path,
                               struct analysis_option option,
                               struct analysis_param params)
            : parser_(parser),
              output_path_(std::move(output_path)),
              option_(option),
              access_pattern_sample_ratio_inv_(
                  params.access_pattern_sample_ratio_inv),
              track_n_popular_(params.track_n_popular),
              track_n_hit_(params.track_n_hit),
              time_window_(params.time_window),
              warmup_time_(params.warmup_time)
        {
            if (warmup_time_ % time_window_ != 0)
            {
                /* the popularityDecay computation needs warmup time to be multiple of
                 * time_window */
                ERROR("warmup time needs to be multiple of time_window\n");
                exit(1);
            }

            initialize();
        };

        ~TraceAnalyzer() { cleanup(); };

        void run();

        void initialize();

        void cleanup();

        friend ostream &operator<<(ostream &os, const TraceAnalyzer &stat)
        {
            if (!stat.has_run_)
            {
                os << "trace stat has not been computed" << endl;
            }
            else
            {
                os << stat.stat_ss_.str() << std::endl;
            }
            return os;
        }

        /* params */
        int time_window_;
        // warmup time in seconds
        int warmup_time_;
        // the number of requests to the most popular object, 2nd most popular ...
        int track_n_popular_;
        // track one-hit wonders, two-hit wonders, etc.
        int track_n_hit_;
        // the sampling ratio used in access pattern analysis
        int access_pattern_sample_ratio_inv_;

        /* stat */
        int64_t n_req_ = 0;

        /* number of one-hit, two-hit ... */
        uint64_t *n_hit_cnt_ = nullptr;
        /* the number of requests to the most, 2nd most ... popular object */
        uint64_t *popular_cnt_ = nullptr;

        int64_t start_ts_ = 0, end_ts_ = 0;
        /* request size and object size weighted by request and object */
        uint64_t sum_obj_size_req = 0, sum_obj_size_obj = 0;
        /* request size can be different from object size when only part of
         * an object is requested, we ignore for now */
        //  uint64_t sum_req_size_req = 0, sum_req_size_obj = 0;

        obj_info_map_type obj_map_; // 对象id->对象元数据

    private:
        parser::Parser *parser_ = nullptr;
        bool has_run_ = false;
        stringstream stat_ss_;
        struct analysis_option option_;

        OpStat *op_stat_ = nullptr;
        TtlStat *ttl_stat_ = nullptr;
        ReqRate *req_rate_stat_ = nullptr;
        SizeDistribution *size_stat_ = nullptr;
        ReuseDistribution *reuse_stat_ = nullptr;
        AccessPattern *access_stat_ = nullptr;
        Popularity *popularity_stat_ = nullptr;
        PopularityDecay *popularity_decay_stat_ = nullptr;
        LifespanDistribution *lifespan_stat_ = nullptr;

        ProbAtAge *prob_at_age_ = nullptr;
        LifetimeDistribution *lifetime_stat_ = nullptr;

        // WriteReuseDistribution *write_reuse_stat_ = nullptr;
        CreateFutureReuseDistribution *create_future_reuse_ = nullptr;
        // WriteFutureReuseDistribution *write_future_reuse_stat_ = nullptr;
        SizeChangeDistribution *size_change_distribution_ = nullptr;
        ScanDetector *scan_detector_ = nullptr;

        string output_path_;

        void post_processing();

        string gen_stat_str();

        inline int time_to_window_idx(uint32_t rtime) { return rtime / time_window_; }
    };

}; // namespace traceAnalyzer
