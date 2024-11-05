//
// Created by Juncheng on 6/5/21.
//

#include <algorithm>  // std::make_heap, std::pop_heap, std::push_heap, std::sort_heap
#include <vector>  // std::vector

#include "analyzer.h"

// 初始化
void traceAnalyzer::TraceAnalyzer::initialize() {
    obj_map_.reserve(DEFAULT_PREALLOC_N_OBJ);  // 预分配空间

    // 对操作类型信息进行统计
    op_stat_ = new OpStat();

    // 是否统计ttl
    if (option_.ttl) {
        ttl_stat_ = new TtlStat();
    }

    // 是否统计请求速率
    if (option_.req_rate) {
        req_rate_stat_ = new ReqRate(time_window_);
    }

    // 是否统计访问模式
    if (option_.access_pattern) {
        access_stat_ = new AccessPattern(access_pattern_sample_ratio_inv_);
    }

    // 是否统计大小分布
    if (option_.size) {
        size_stat_ = new SizeDistribution(output_path_, time_window_);
    }

    // 是否统计重用时间分布
    if (option_.reuse) {
        reuse_stat_ = new ReuseDistribution(output_path_, time_window_);
    }

    // 是否统计过去若干个时间窗口内，每个时间窗口的请求对象数和新对象数
    if (option_.popularity_decay) {
        popularity_decay_stat_ =
            new PopularityDecay(output_path_, time_window_, warmup_time_);
    }

    // 是否统计重用时间分布(各个重用时间的访问次数、访问年龄总和、访问频率总和)
    if (option_.create_future_reuse_ccdf) {
        create_future_reuse_ = new CreateFutureReuseDistribution(warmup_time_);
    }

    // 是否统计 <重用时间经过的时间窗口数，自创建以来经过的时间窗口数>
    // 对应的请求次数 即不同访问年龄的对象被重用的概率
    if (option_.prob_at_age) {
        prob_at_age_ = new ProbAtAge(time_window_, warmup_time_);
    }

    // 是否统计对象生命周期
    if (option_.lifetime) {
        lifetime_stat_ = new LifetimeDistribution();
    }

    // 是否统计覆写对象的大小变化分布情况
    if (option_.size_change) {
        size_change_distribution_ = new SizeChangeDistribution();
    }

    if (option_.lifespan) {
        lifespan_stat_ = new LifespanDistribution(output_path_, time_window_);
    }

    // scan_detector_ = new ScanDetector(reader_, output_path, 100);
}

// 析构
void traceAnalyzer::TraceAnalyzer::cleanup() {
    delete op_stat_;
    delete ttl_stat_;
    delete req_rate_stat_;
    delete reuse_stat_;
    delete size_stat_;
    delete access_stat_;
    delete popularity_stat_;
    delete popularity_decay_stat_;

    delete prob_at_age_;
    delete lifetime_stat_;
    delete create_future_reuse_;
    delete size_change_distribution_;
    delete lifespan_stat_;

    // delete write_reuse_stat_;
    // delete write_future_reuse_stat_;

    delete scan_detector_;

    if (n_hit_cnt_ != nullptr) {
        delete[] n_hit_cnt_;
    }
    if (popular_cnt_ != nullptr) {
        delete[] popular_cnt_;
    }
}

// 读取请求，进行分析
void traceAnalyzer::TraceAnalyzer::run() {
    if (has_run_)
        return;

    // 先读取一个请求
    parser::Request* req = new parser::Request;
    parser_->read_one_req(req);

    // 起始时间赋值为第一个请求的时间戳
    start_ts_ = req->time;
    int32_t curr_time_window_idx = 0;
    int next_time_window_ts = time_window_;

    int64_t n = 0;
    /* going through the trace */
    do {
        DEBUG_ASSERT(req->req_size != 0);

        // change real time to relative time
        // 将请求的实际时间戳转换为相对于起始时间的实际时间戳
        req->time -= start_ts_;

        // 按实际时间戳划分时间窗口
        while (req->time >= next_time_window_ts) {
            curr_time_window_idx += 1;
            next_time_window_ts += time_window_;
        }

        // 检查请求的时间戳是否按顺序排列
        if (curr_time_window_idx != time_to_window_idx(req->time)) {
            ERROR(
                "The data is not ordered by time, please sort the trace first!"
                "Current time %ld requested object %lu, obj size %lu\n",
                (long)(req->time + start_ts_), (unsigned long)req->id,
                (long)req->req_size);
        }

        DEBUG_ASSERT(curr_time_window_idx == time_to_window_idx(req->time));

        n_req_ += 1;
        req->req_num = n_req_;

        if (n_req_ % 1000000 == 0) {
            printf("Processed %ld requests\n", n_req_);
        }

        sum_obj_size_req += req->req_size;

        auto it = obj_map_.find(req->id);
        // 根据当前对象是否出现过，初始化一些请求和对象信息
        if (it == obj_map_.end()) {
            /* the first request to the object */

            // 当前请求标记为强制不命中
            req->compulsory_miss =
                true; /* whether the object is seen for the first time */
            // 新对象，非覆写
            req->overwrite = false;
            // 当前对象在当前时间窗口是第一次出现
            req->first_seen_in_window = true;
            // 当前请求的对象的创建时间为当前请求的时间戳
            req->create_rtime = (int32_t)req->time;
            req->create_vtime = (int32_t)req->req_num;

            // 当前对象之前的大小
            req->prev_size = -1;
            // req->last_seen_window_idx = curr_time_window_idx;

            // 当前对象的访问时间间隔(逻辑时间和实际时间)
            req->vtime_since_last_access = -1;
            req->rtime_since_last_access = -1;
            req->vtime_since_last_update = -1;
            req->rtime_since_last_update = -1;

            struct obj_info obj_info;
            // 对象的创建时间
            obj_info.create_rtime = (int32_t)req->time;
            obj_info.create_vtime = (int32_t)req->req_num;

            // 对象的访问频次
            obj_info.freq = 1;
            // 对象的大小
            obj_info.obj_size = (obj_size_t)req->req_size;
            // 对象的最后访问时间(逻辑时间和实际时间)
            obj_info.last_access_rtime = (int32_t)req->time;
            obj_info.last_access_vtime = n_req_;

            obj_info.last_update_rtime = (int32_t)req->time;
            obj_info.last_update_vtime = (int32_t)req->req_num;

            // 将对象信息加入到对象map中
            obj_map_[req->id] = obj_info;
            // 不重复对象的总大小，WSS
            sum_obj_size_obj += req->req_size;
        } else  // 当前对象出现过
        {
            // 非强制不命中
            req->compulsory_miss = false;
            // 当前对象在当前时间窗口是否第一次出现
            req->first_seen_in_window =
                (time_to_window_idx(it->second.last_access_rtime) !=
                 curr_time_window_idx);
            // 当前请求的对象的创建时间

            req->create_rtime = it->second.create_rtime;
            req->create_vtime = it->second.create_vtime;

            // 计算当前请求的对象的更新时间间隔(逻辑时间和实际时间)
            req->rtime_since_last_update =
                (int64_t)req->time - it->second.last_update_rtime;
            req->vtime_since_last_update =
                (int64_t)req->req_num - it->second.last_update_vtime;

            // 计算当前请求的对象的访问时间间隔(逻辑时间和实际时间)
            req->vtime_since_last_access =
                (int64_t)n_req_ - it->second.last_access_vtime;
            req->rtime_since_last_access =
                (int64_t)(req->time) - it->second.last_access_rtime;

            // 若为写入，则是覆写请求
            if (req->type == parser::OP_SET ||
                req->type == parser::OP_REPLACE ||
                req->type == parser::OP_CAS) {
                req->overwrite = true;

                it->second.last_update_rtime = (int32_t)req->time;
                it->second.last_update_vtime = (int32_t)req->req_num;
            } else  // 否则不是覆写请求
            {
                req->overwrite = false;
            }

            assert(req->vtime_since_last_access > 0);
            assert(req->rtime_since_last_access >= 0);

            // 对象之前的大小
            req->prev_size = it->second.obj_size;
            // 对象的当前的大小
            it->second.obj_size = req->req_size;
            // 对象的访问频次加1
            it->second.freq += 1;
            // 对象的最后访问时间(逻辑时间和实际时间)
            it->second.last_access_vtime = n_req_;
            it->second.last_access_rtime = (int32_t)(req->time);
        }

        // 将当前请求计入到各个统计中

        op_stat_->add_req(req);

        if (ttl_stat_ != nullptr) {
            ttl_stat_->add_req(req);
        }

        if (req_rate_stat_ != nullptr) {
            req_rate_stat_->add_req(req);
        }

        if (size_stat_ != nullptr) {
            size_stat_->add_req(req);
        }

        if (reuse_stat_ != nullptr) {
            reuse_stat_->add_req(req);
        }

        if (access_stat_ != nullptr) {
            access_stat_->add_req(req);
        }

        if (popularity_decay_stat_ != nullptr) {
            popularity_decay_stat_->add_req(req);
        }

        if (prob_at_age_ != nullptr) {
            prob_at_age_->add_req(req);
        }

        if (lifetime_stat_ != nullptr) {
            lifetime_stat_->add_req(req);
        }

        if (create_future_reuse_ != nullptr) {
            create_future_reuse_->add_req(req);
        }

        if (size_change_distribution_ != nullptr) {
            size_change_distribution_->add_req(req);
        }
        if (scan_detector_ != nullptr) {
            scan_detector_->add_req(req);
        }

        if (lifespan_stat_ != nullptr) {
            lifespan_stat_->add_req(req);
        }

        // 读取下一个请求，直到读取到的请求无效，即读取到文件末尾
        parser_->read_one_req(req);
    } while (req->valid);

    // 结束时间戳赋值为最后一个请求的时间戳
    end_ts_ = req->time + start_ts_;

    /* processing */
    // 统计访问频次分布和对象流行度遵循的zipf分布参数
    post_processing();

    free_request(req);

    ofstream ofs("stat", ios::out | ios::app);
    ofs << gen_stat_str() << endl;
    ofs.close();

    if (ttl_stat_ != nullptr) {
        ttl_stat_->dump(output_path_);
    }

    if (req_rate_stat_ != nullptr) {
        req_rate_stat_->dump(output_path_);
    }

    if (reuse_stat_ != nullptr) {
        reuse_stat_->dump(output_path_);
    }

    if (size_stat_ != nullptr) {
        size_stat_->dump(output_path_);
    }

    if (access_stat_ != nullptr) {
        access_stat_->dump(output_path_);
    }

    if (popularity_stat_ != nullptr) {
        popularity_stat_->dump(output_path_);
    }

    if (popularity_decay_stat_ != nullptr) {
        popularity_decay_stat_->dump(output_path_);
    }

    if (prob_at_age_ != nullptr) {
        prob_at_age_->dump(output_path_);
    }

    if (lifetime_stat_ != nullptr) {
        lifetime_stat_->dump(output_path_);
    }

    if (create_future_reuse_ != nullptr) {
        create_future_reuse_->dump(output_path_);
    }

    // if (write_reuse_stat_ != nullptr) {
    //   write_reuse_stat_->dump(output_path_);
    // }

    // if (write_future_reuse_stat_ != nullptr) {
    //   write_future_reuse_stat_->dump(output_path_);
    // }

    if (scan_detector_ != nullptr) {
        scan_detector_->dump(output_path_);
    }

    if (lifespan_stat_ != nullptr) {
        lifespan_stat_->dump(output_path_);
    }

    has_run_ = true;
}

string traceAnalyzer::TraceAnalyzer::gen_stat_str() {
    // 清空字符串流
    stat_ss_.clear();
    // 强制对象缺失率
    double cold_miss_ratio = (double)obj_map_.size() / (double)n_req_;
    // 强制字节缺失率
    double byte_cold_miss_ratio =
        (double)sum_obj_size_obj / (double)sum_obj_size_req;
    // 请求大小(包含重复对象)和对象大小(不包含重复对象)的平均值
    int mean_obj_size_req = (int)((double)sum_obj_size_req / (double)n_req_);
    int mean_obj_size_obj =
        (int)((double)sum_obj_size_obj / (double)obj_map_.size());
    // 每个对象的平均访问频次
    double freq_mean = (double)n_req_ / (double)obj_map_.size();
    // 实际时间戳跨度
    int64_t time_span = end_ts_ - start_ts_;

    stat_ss_ << setprecision(4) << fixed << "dat: " << parser_->trace_path_
             << "\n"
             << "number of requests: " << n_req_
             << ", number of objects: " << obj_map_.size() << "\n"
             << "number of req GiB: " << (double)sum_obj_size_req / (double)GiB
             << ", number of obj GiB: "
             << (double)sum_obj_size_obj / (double)GiB << "\n"
             << "compulsory miss ratio (req/byte): " << cold_miss_ratio << "/"
             << byte_cold_miss_ratio << "\n"
             << "object size weighted by req/obj: " << mean_obj_size_req << "/"
             << mean_obj_size_obj << "\n"
             << "frequency mean: " << freq_mean << "\n";
    stat_ss_ << "time span: " << time_span << "("
             << (double)(end_ts_ - start_ts_) / 3600 / 24 << " day)\n";

    // 输出各个统计信息
    stat_ss_ << *op_stat_;
    if (ttl_stat_ != nullptr) {
        stat_ss_ << *ttl_stat_;
    }
    if (req_rate_stat_ != nullptr)
        stat_ss_ << *req_rate_stat_;
    if (popularity_stat_ != nullptr)
        stat_ss_ << *popularity_stat_;

    stat_ss_ << "X-hit (number of obj accessed X times): ";
    for (int i = 0; i < track_n_hit_; i++) {
        stat_ss_ << n_hit_cnt_[i] << "("
                 << (double)n_hit_cnt_[i] / (double)obj_map_.size() << "), ";
    }
    stat_ss_ << "\n";

    stat_ss_ << "freq (fraction) of the most popular obj: ";
    for (int i = 0; i < track_n_popular_; i++) {
        stat_ss_ << popular_cnt_[i] << "("
                 << (double)popular_cnt_[i] / (double)n_req_ << "), ";
    }
    stat_ss_ << "\n";

    if (size_change_distribution_ != nullptr)
        stat_ss_ << *size_change_distribution_;

    if (scan_detector_ != nullptr)
        stat_ss_ << *scan_detector_;

    return stat_ss_.str();
}

void traceAnalyzer::TraceAnalyzer::post_processing() {
    assert(n_hit_cnt_ == nullptr);
    assert(popular_cnt_ == nullptr);

    n_hit_cnt_ = new uint64_t[track_n_hit_];
    popular_cnt_ = new uint64_t[track_n_popular_];
    memset(n_hit_cnt_, 0, sizeof(uint64_t) * track_n_hit_);
    memset(popular_cnt_, 0, sizeof(uint64_t) * track_n_popular_);

    // 统计对象的访问频次分布
    for (auto it : obj_map_) {
        if (it.second.freq <= track_n_hit_) {
            n_hit_cnt_[it.second.freq - 1] += 1;
        }
    }

    // 统计对象流行度，统计包括访问频率和排名遵从的zipf分布参数，以及各个访问频率的出现次数
    if (option_.popularity) {
        popularity_stat_ = new Popularity(obj_map_);
        // 获取对象访问频率的排序列表
        auto sorted_freq = popularity_stat_->get_sorted_freq();
        // 统计最受欢迎的n个对象的访问频率
        for (int i = 0; i < track_n_popular_; i++) {
            popular_cnt_[i] = sorted_freq[i];
        }
    }
}
