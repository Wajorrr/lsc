#include "lifespan.h"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace traceAnalyzer
{
    using namespace std;

    void LifespanDistribution::add_req(parser::Request *req)
    {
        // 在第一个时间窗口时初始化下一个时间窗口的时间戳，赋值为第一个请求的时间戳+时间窗口大小
        if (unlikely(next_window_ts_ == -1))
        {
            next_window_ts_ = (int64_t)req->time + time_window_;
        }

        int rlifetime;
        int vlifetime;
        int rlifetime_since_last_update;
        int vlifetime_since_last_update;

        // 若当前请求的对象第一次出现，重用距离为-1，无穷大
        if (req->rtime_since_last_access < 0)
        {
            // reuse_rtime_req_cnt_[-1] += 1;
            // reuse_vtime_req_cnt_[-1] += 1;
            // return;
            rlifetime = -1;
            vlifetime = -1;
            rlifetime_since_last_update = -1;
            vlifetime_since_last_update = -1;
        }

        // 实际的重用时间间隔，除以一个实际时间粒度
        // int pos_rt = (int)(req->rtime_since_last_access / rtime_granularity_);
        // 逻辑重用时间，取对数
        // int pos_vt = (int)(log(double(req->vtime_since_last_access)) / log_log_base_);

        // 统计出现频率
        // reuse_rtime_req_cnt_[pos_rt] += 1;
        // reuse_vtime_req_cnt_[pos_vt] += 1;

        // 从对象创建以来的生命周期
        rlifetime = req->time - req->create_rtime;
        vlifetime = req->req_num - req->create_vtime;
        if (rlifetime == 0)
            rlifetime = -1;
        if (vlifetime == 0)
            vlifetime = -1;

        // 上一次更新以来的生命周期
        rlifetime_since_last_update = req->rtime_since_last_update;
        vlifetime_since_last_update = req->vtime_since_last_update;

        switch (req->type)
        {
        // 读取操作
        case parser::OP_GET:
        case parser::OP_GETS:
        case parser::OP_HEAD:
            // reuse_rtime_req_cnt_read_[pos_rt] += 1;
            rlifespan_readprob[rlifetime] += 1;
            rlifespan2_readprob[rlifetime_since_last_update] += 1;
            vlifespan_readprob[vlifetime] += 1;
            vlifespan2_readprob[vlifetime_since_last_update] += 1;
            break;
        // 写入操作
        case parser::OP_SET:
        case parser::OP_ADD:
        case parser::OP_REPLACE:
        case parser::OP_CAS:
            // reuse_rtime_req_cnt_write_[pos_rt] += 1;
            rlifespan_writeprob[rlifetime] += 1;
            rlifespan2_writeprob[rlifetime_since_last_update] += 1;
            vlifespan_writeprob[vlifetime] += 1;
            vlifespan2_writeprob[vlifetime_since_last_update] += 1;
            break;
        case parser::OP_DELETE:
            // reuse_rtime_req_cnt_delete_[pos_rt] += 1;
            rlifespan_deleteprob[rlifetime] += 1;
            rlifespan2_deleteprob[rlifetime_since_last_update] += 1;
            vlifespan_deleteprob[vlifetime] += 1;
            vlifespan2_deleteprob[vlifetime_since_last_update] += 1;
            break;
        default:
            break;
        }

        if (time_window_ <= 0)
            return;

        // 统计每个时间窗口的重用时间间隔分布
        // utils::vector_incr(window_reuse_rtime_req_cnt_, pos_rt, (uint32_t)1);
        // utils::vector_incr(window_reuse_vtime_req_cnt_, pos_vt, (uint32_t)1);

        // 若当前时间戳超过下一个时间窗口的时间戳
        while (req->time >= next_window_ts_)
        {
            // 输出当前时间窗口的重用时间间隔分布
            // stream_dump_window_reuse_distribution();
            // 重置
            // window_reuse_rtime_req_cnt_.clear();
            // window_reuse_vtime_req_cnt_.clear();
            next_window_ts_ += time_window_;
        }
    }

    void LifespanDistribution::dump(string &path_base)
    {
        ofstream ofs1(path_base + ".read_rlifespan", ios::out | ios::trunc);
        ofs1 << "# " << path_base << "\n";

        ofs1 << "# read rlifespan1\n";
        for (auto &p : rlifespan_readprob)
        {
            ofs1 << p.first << ":" << p.second << "\n";
        }

        ofs1 << "# read rlifespan2\n";
        for (auto &p : rlifespan2_readprob)
        {
            ofs1 << p.first << ":" << p.second << "\n";
        }

        ofs1.close();

        ofstream ofs2(path_base + ".read_vlifespan", ios::out | ios::trunc);
        ofs2 << "# " << path_base << "\n";

        ofs2 << "# read vlifespan1\n";
        for (auto &p : vlifespan_readprob)
        {
            ofs2 << p.first << ":" << p.second << "\n";
        }

        ofs2 << "# read vlifespan2\n";
        for (auto &p : vlifespan2_readprob)
        {
            ofs2 << p.first << ":" << p.second << "\n";
        }

        ofs2.close();

        ofstream ofs3(path_base + ".write_rlifespan", ios::out | ios::trunc);
        ofs3 << "# " << path_base << "\n";

        ofs3 << "# write rlifespan1\n";
        for (auto &p : rlifespan_writeprob)
        {
            ofs3 << p.first << ":" << p.second << "\n";
        }

        ofs3 << "# write rlifespan2\n";
        for (auto &p : rlifespan2_writeprob)
        {
            ofs3 << p.first << ":" << p.second << "\n";
        }

        ofs3.close();

        ofstream ofs4(path_base + ".write_vlifespan", ios::out | ios::trunc);
        ofs4 << "# " << path_base << "\n";

        ofs4 << "# write vlifespan1\n";
        for (auto &p : vlifespan_writeprob)
        {
            ofs4 << p.first << ":" << p.second << "\n";
        }

        ofs4 << "# write vlifespan2\n";
        for (auto &p : vlifespan2_writeprob)
        {
            ofs4 << p.first << ":" << p.second << "\n";
        }

        ofs4.close();

        ofstream ofs5(path_base + ".delete_rlifespan", ios::out | ios::trunc);
        ofs5 << "# " << path_base << "\n";

        ofs5 << "# delete rlifespan1\n";
        for (auto &p : rlifespan_deleteprob)
        {
            ofs5 << p.first << ":" << p.second << "\n";
        }

        ofs5 << "# delete rlifespan2\n";
        for (auto &p : rlifespan2_deleteprob)
        {
            ofs5 << p.first << ":" << p.second << "\n";
        }

        ofs5.close();

        ofstream ofs6(path_base + ".delete_vlifespan", ios::out | ios::trunc);
        ofs6 << "# " << path_base << "\n";

        ofs6 << "# delete vlifespan1\n";
        for (auto &p : vlifespan_deleteprob)
        {
            ofs6 << p.first << ":" << p.second << "\n";
        }

        ofs6 << "# delete vlifespan2\n";
        for (auto &p : vlifespan2_deleteprob)
        {
            ofs6 << p.first << ":" << p.second << "\n";
        }

        ofs6.close();

        //    if (std::accumulate(reuse_rtime_req_cnt_read_.begin(),
        //    reuse_rtime_req_cnt_read_.end(), 0) == 0)
        //      return;
        //
        //    ofs.open(path_base + ".reuse_op", ios::out | ios::trunc);
        //    ofs << "# " << path_base << "\n";
        //    ofs << "# reuse real time: freq (time granularity " <<
        //    rtime_granularity_ << ")\n";
    }

    void LifespanDistribution::turn_on_stream_dump(string &path_base)
    {
        // 打开实际重用时间间隔分布要输出到的文件
        stream_dump_rt_ofs.open(
            path_base + ".lifespanWindow_w" + to_string(time_window_) + "_rt",
            ios::out | ios::trunc);
        stream_dump_rt_ofs << "# " << path_base << "\n";
        stream_dump_rt_ofs
            << "# lifespan real time distribution per window (time granularity ";
        stream_dump_rt_ofs << rtime_granularity_ << ", time window " << time_window_
                           << ")\n";

        // 打开逻辑重用时间间隔分布要输出到的文件
        stream_dump_vt_ofs.open(
            path_base + ".lifespanWindow_w" + to_string(time_window_) + "_vt",
            ios::out | ios::trunc);
        stream_dump_vt_ofs << "# " << path_base << "\n";
        stream_dump_vt_ofs
            << "# lifespan virtual time distribution per window (log base " << log_base_;
        stream_dump_vt_ofs << ", time window " << time_window_ << ")\n";
    }

    void LifespanDistribution::stream_dump_window_reuse_distribution()
    {
        // 输出一段时间窗口内的实际重用时间间隔分布
        // for (const auto &p : window_reuse_rtime_req_cnt_)
        // {
        //   stream_dump_rt_ofs << p << ",";
        // }
        // stream_dump_rt_ofs << "\n";

        // 输出一段时间窗口内的逻辑重用时间间隔分布
        // for (const auto &p : window_reuse_vtime_req_cnt_)
        // {
        //   stream_dump_vt_ofs << p << ",";
        // }
        // stream_dump_vt_ofs << "\n";
    }
}; // namespace traceAnalyzer
