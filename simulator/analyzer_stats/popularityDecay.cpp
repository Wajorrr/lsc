
#include "popularityDecay.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace traceAnalyzer
{
    using namespace std;

    void PopularityDecay::turn_on_stream_dump(string &path_base)
    {
#ifdef USE_REQ_METRIC
        // 打开请求对象的统计信息输出文件
        stream_dump_req_ofs.open(
            path_base + ".popularityDecay_w" + to_string(time_window_) + "_req",
            ios::out | ios::trunc);
        stream_dump_req_ofs << "# " << path_base << "\n";
        stream_dump_req_ofs
            << "# req_cnt for new object in prev N windows (time window "
            << time_window_ << ")\n";
#endif

        // 打开第一次出现对象的统计信息输出文件
        stream_dump_obj_ofs.open(
            path_base + ".popularityDecay_w" + to_string(time_window_) + "_obj",
            ios::out | ios::trunc);
        stream_dump_obj_ofs << "# " << path_base << "\n";
        stream_dump_obj_ofs
            << "# obj_cnt for new object in prev N windows (time window "
            << time_window_ << ")\n";
    }

    void PopularityDecay::add_req(const parser::Request *req)
    {
        // 在第一个时间窗口时初始化下一个时间窗口的时间戳，赋值为第一个请求的时间戳+时间窗口大小
        if (unlikely(next_window_ts_ == -1))
        {
            next_window_ts_ = (int64_t)req->time + time_window_;
        }

        /* this assumes req real time starts from 0 */
        // 预热时间
        if (req->time < warmup_rtime_)
        {
            while (req->time >= next_window_ts_)
            {
                next_window_ts_ += time_window_;
            }
            return;
        }

        // 计算当前请求创建对象时的时间窗口索引
        int create_time_window_idx = time_to_window_idx(req->create_rtime);
        if (create_time_window_idx < idx_shift)
        {
            // the object is created during warm up
            return;
        }

        // 下一个时间窗口
        while (req->time >= next_window_ts_)
        {
            // 若还在预热时间内
            if (next_window_ts_ < warmup_rtime_)
            {
                /* if the timestamp jumped (no requests in some time windows) */
                next_window_ts_ = warmup_rtime_;
                continue;
            }
            /* because req->time < warmup_rtime_ not <= on line 97,
             * the first line of output will be 0, and the last of each line is 0 */
            // 输出
            stream_dump();
            // 重置
            n_req_per_window.assign(n_req_per_window.size() + 1, 0);
            n_obj_per_window.assign(n_obj_per_window.size() + 1, 0);
            next_window_ts_ += time_window_;
        }

        assert(create_time_window_idx - idx_shift < n_req_per_window.size());

        // 对应时间窗口的请求数+1
        n_req_per_window.at(create_time_window_idx - idx_shift) += 1;
        // 若对象为第一次出现，则对应时间窗口的第一次出现对象数+1
        if (req->first_seen_in_window)
        {
            n_obj_per_window.at(create_time_window_idx - idx_shift) += 1;
        }
    }

}; // namespace traceAnalyzer
