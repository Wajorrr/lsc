
#include "size.h"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <string>

namespace traceAnalyzer
{
    using namespace std;

    void SizeDistribution::add_req(parser::Request *req)
    {
        // 在第一个时间窗口时初始化下一个时间窗口的时间戳，赋值为第一个请求的时间戳+时间窗口大小
        if (unlikely(next_window_ts_ == -1))
        {
            next_window_ts_ = (int64_t)req->time + time_window_;
        }

        /* request count */
        // 请求的对象大小出现次数分布
        obj_size_req_cnt_[req->req_size] += 1;

        /* object count */
        // 不重复对象的大小分布
        if (req->compulsory_miss)
        {
            obj_size_obj_cnt_[req->req_size] += 1;
        }

        if (time_window_ <= 0)
            return;

        /* window request/object count is stored using vector */
        // MAX(log_{LOG_BASE}{(req->req_size / SIZE_BASE)} , 0);
        //      int pos = MAX((int) (log((double) req->req_size / SIZE_BASE) /
        //      log(LOG_BASE)), 0);

        // 计算对象大小的对数，底为LOG_BASE (log_log_base = log(LOG_BASE))
        // 取整
        int pos = (int)MAX(log((double)req->req_size) / log_log_base, 0);

        // 若超出了桶的大小，扩容
        if (pos >= window_obj_size_req_cnt_.size())
        {
            window_obj_size_req_cnt_.resize(pos + 8, 0);
            window_obj_size_obj_cnt_.resize(pos + 8, 0);
        }

        /* window request count */
        // 统计请求大小的出现频率
        window_obj_size_req_cnt_[pos] += 1;

        /* window object count */
        //    if (window_seen_obj.count(req->obj_id) == 0) {
        //      window_obj_size_obj_cnt_[pos] += 1;
        //      window_seen_obj.insert(req->obj_id);
        //    }

        // 统计时间窗口内第一次出现的对象的大小分布
        if (req->first_seen_in_window)
        {
            window_obj_size_obj_cnt_[pos] += 1;
        }

        // 若当前请求的时间戳超过了下一个时间窗口的时间戳
        while (req->time >= next_window_ts_)
        {
            // 输出当前窗口的统计结果
            stream_dump();
            // 重置
            window_obj_size_req_cnt_ = vector<uint32_t>(20, 0);
            window_obj_size_obj_cnt_ = vector<uint32_t>(20, 0);
            next_window_ts_ += time_window_;
        }
    }

    // 输出总体的统计信息
    void SizeDistribution::dump(string &path_base)
    {
        ofstream ofs(path_base + ".size", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";

        // 输出请求的对象大小分布
        ofs << "# object_size: req_cnt\n";
        for (auto &p : obj_size_req_cnt_)
        {
            ofs << p.first << ":" << p.second << "\n";
        }

        // 输出第一次出现的对象的大小分布
        ofs << "# object_size: obj_cnt\n";
        for (auto &p : obj_size_obj_cnt_)
        {
            ofs << p.first << ":" << p.second << "\n";
        }
        ofs.close();
    }

    void SizeDistribution::turn_on_stream_dump(string &path_base)
    {
        // 请求大小的对数分布文件
        // 打开输出文件，ios::trunc表示如果文件已经存在，它的内容将被清空
        ofs_stream_req.open(
            path_base + ".sizeWindow_w" + to_string(time_window_) + "_req",
            ios::out | ios::trunc);
        ofs_stream_req << "# " << path_base << "\n";
        // 输出时间窗口长度以及对数的底
        ofs_stream_req << "# object_size: req_cnt (time window " << time_window_
                       << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";

        // 第一次出现的对象的对数大小分布文件
        ofs_stream_obj.open(
            path_base + ".sizeWindow_w" + to_string(time_window_) + "_obj",
            ios::out | ios::trunc);
        ofs_stream_obj << "# " << path_base << "\n";
        ofs_stream_obj << "# object_size: obj_cnt (time window " << time_window_
                       << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";
    }

    // 输出一段时间窗口内的统计信息(对象大小取对数)
    void SizeDistribution::stream_dump()
    {
        // 输出请求大小的对数分布
        for (const auto &p : window_obj_size_req_cnt_)
        {
            ofs_stream_req << p << ",";
        }
        ofs_stream_req << "\n";

        // 输出第一次出现的对象的对数大小分布
        for (const auto &p : window_obj_size_obj_cnt_)
        {
            ofs_stream_obj << p << ",";
        }
        ofs_stream_obj << "\n";
    }
}; // namespace traceAnalyzer
