#include "reqRate.h"

namespace traceAnalyzer
{

    using namespace std;

    void ReqRate::add_req(parser::Request *req)
    {
        // 在第一个时间窗口时初始化下一个时间窗口的时间戳，赋值为第一个请求的时间戳+时间窗口大小
        if (unlikely(next_window_ts_ == -1))
        {
            next_window_ts_ = (int64_t)req->time + time_window_;
        }

        assert(next_window_ts_ != -1);

        // 时间窗口内的总请求数和总请求字节数
        window_n_req_ += 1;
        window_n_byte_ += req->req_size;

        // 时间窗口内第一次出现的对象数
        if (req->first_seen_in_window)
            window_n_obj_ += 1;

        //    if (window_seen_obj_.find(req->obj_id) == window_seen_obj_.end()) {
        //      window_seen_obj_.insert(req->obj_id);
        //    }

        // 冷不命中次数
        if (req->compulsory_miss)
        {
            window_compulsory_miss_obj_ += 1;
        }

        // 判断是否进入下一个时间窗口
        while (req->time >= next_window_ts_)
        {
            // 将当前时间窗口的统计数据存入对应的数组中
            req_rate_.push_back(window_n_req_);   // 各个时间窗口内的总请求数
            byte_rate_.push_back(window_n_byte_); // 各个时间窗口内的总字节请求数
            obj_rate_.push_back(window_n_obj_);   // 各个时间窗口内的新对象请求数
            // 各个时间窗口内的冷不命中对象请求数
            first_seen_obj_rate_.push_back(window_compulsory_miss_obj_);

            // 重置时间窗口内的统计数据
            window_n_req_ = 0;
            window_n_byte_ = 0;
            window_n_obj_ = 0;
            window_compulsory_miss_obj_ = 0;
            // window_seen_obj_.clear();
            next_window_ts_ += time_window_;
        }
    }

    void ReqRate::dump(const string &path_base)
    {
        ofstream ofs(path_base + ".reqRate_w" + to_string(time_window_),
                     ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";

        // 输出各个时间窗口内的请求速率(个/s)
        ofs << "# req rate - time window " << time_window_ << " second\n";
        for (auto &n_req : req_rate_)
        {
            ofs << n_req / time_window_ << ",";
        }
        ofs << "\n";

        // 输出各个时间窗口内的字节速率(bytes/s)
        ofs << "# byte rate - time window " << time_window_ << " second\n";
        for (auto &n_byte : byte_rate_)
        {
            ofs << n_byte / time_window_ << ",";
        }
        ofs << "\n";

        // 输出各个时间窗口内的新对象请求速率(个/s)
        ofs << "# obj rate - time window " << time_window_ << " second\n";
        for (auto &n_obj : obj_rate_)
        {
            ofs << n_obj / time_window_ << ",";
        }
        ofs << "\n";

        // 输出各个时间窗口内的冷不命中对象请求速率(个/s)
        ofs << "# first seen obj (cold miss) rate - time window " << time_window_
            << " second\n";
        for (auto &n_obj : first_seen_obj_rate_)
        {
            ofs << n_obj / time_window_ << ",";
        }
        ofs << "\n";

        ofs.close();
    }

}; // namespace traceAnalyzer
