
#include "reuse.h"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/macro.h"

namespace traceAnalyzer
{
    using namespace std;

    void ReuseDistribution::add_req(parser::Request *req)
    {
        // 在第一个时间窗口时初始化下一个时间窗口的时间戳，赋值为第一个请求的时间戳+时间窗口大小
        if (unlikely(next_window_ts_ == -1))
        {
            next_window_ts_ = (int64_t)req->time + time_window_;
        }

        // 若当前请求的对象第一次出现，重用距离为-1，无穷大
        if (req->rtime_since_last_access < 0)
        {
            reuse_rtime_req_cnt_[-1] += 1;
            reuse_vtime_req_cnt_[-1] += 1;

            return;
        }

        // 实际的重用时间间隔，除以一个实际时间粒度
        int pos_rt = (int)(req->rtime_since_last_access / rtime_granularity_);
        // 逻辑重用时间，取对数
        int pos_vt = (int)(log(double(req->vtime_since_last_access)) / log_log_base_);

        // 统计出现频率
        reuse_rtime_req_cnt_[pos_rt] += 1;
        reuse_vtime_req_cnt_[pos_vt] += 1;

        //    switch (req->op) {
        //      case OP_GET:
        //      case OP_GETS:
        //        reuse_rtime_req_cnt_read_[pos_rt] += 1;
        //        break;
        //      case OP_SET:
        //      case OP_ADD:
        //      case OP_REPLACE:
        //      case OP_CAS:
        //        reuse_rtime_req_cnt_write_[pos_rt] += 1;
        //        break;
        //      case OP_DELETE:
        //        reuse_rtime_req_cnt_delete_[pos_rt] += 1;
        //        break;
        //      default:
        //        break;
        //    }

        if (time_window_ <= 0)
            return;

        // 统计每个时间窗口的重用时间间隔分布
        utils::vector_incr(window_reuse_rtime_req_cnt_, pos_rt, (uint32_t)1);
        utils::vector_incr(window_reuse_vtime_req_cnt_, pos_vt, (uint32_t)1);

        // 若当前时间戳超过下一个时间窗口的时间戳
        while (req->time >= next_window_ts_)
        {
            // 输出当前时间窗口的重用时间间隔分布
            stream_dump_window_reuse_distribution();
            // 重置
            window_reuse_rtime_req_cnt_.clear();
            window_reuse_vtime_req_cnt_.clear();
            next_window_ts_ += time_window_;
        }
    }

    void ReuseDistribution::dump(string &path_base)
    {
        ofstream ofs(path_base + ".reuse", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";

        // 总体的实际重用时间间隔分布
        ofs << "# reuse real time: freq (time granularity " << rtime_granularity_
            << ")\n";
        for (auto &p : reuse_rtime_req_cnt_)
        {
            ofs << p.first << ":" << p.second << "\n";
        }

        // 总体的逻辑重用时间间隔分布
        ofs << "# reuse virtual time: freq (log base " << log_base_ << ")\n";
        for (auto &p : reuse_vtime_req_cnt_)
        {
            ofs << p.first << ":" << p.second << "\n";
        }
        ofs.close();

        //    if (std::accumulate(reuse_rtime_req_cnt_read_.begin(),
        //    reuse_rtime_req_cnt_read_.end(), 0) == 0)
        //      return;
        //
        //    ofs.open(path_base + ".reuse_op", ios::out | ios::trunc);
        //    ofs << "# " << path_base << "\n";
        //    ofs << "# reuse real time: freq (time granularity " <<
        //    rtime_granularity_ << ")\n";
    }

    void ReuseDistribution::turn_on_stream_dump(string &path_base)
    {
        // 打开实际重用时间间隔分布要输出到的文件
        stream_dump_rt_ofs.open(
            path_base + ".reuseWindow_w" + to_string(time_window_) + "_rt",
            ios::out | ios::trunc);
        stream_dump_rt_ofs << "# " << path_base << "\n";
        stream_dump_rt_ofs
            << "# reuse real time distribution per window (time granularity ";
        stream_dump_rt_ofs << rtime_granularity_ << ", time window " << time_window_
                           << ")\n";

        // 打开逻辑重用时间间隔分布要输出到的文件
        stream_dump_vt_ofs.open(
            path_base + ".reuseWindow_w" + to_string(time_window_) + "_vt",
            ios::out | ios::trunc);
        stream_dump_vt_ofs << "# " << path_base << "\n";
        stream_dump_vt_ofs
            << "# reuse virtual time distribution per window (log base " << log_base_;
        stream_dump_vt_ofs << ", time window " << time_window_ << ")\n";
    }

    void ReuseDistribution::stream_dump_window_reuse_distribution()
    {
        // 输出一段时间窗口内的实际重用时间间隔分布
        for (const auto &p : window_reuse_rtime_req_cnt_)
        {
            stream_dump_rt_ofs << p << ",";
        }
        stream_dump_rt_ofs << "\n";

        // 输出一段时间窗口内的逻辑重用时间间隔分布
        for (const auto &p : window_reuse_vtime_req_cnt_)
        {
            stream_dump_vt_ofs << p << ",";
        }
        stream_dump_vt_ofs << "\n";
    }
}; // namespace traceAnalyzer
