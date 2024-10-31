/**
 * @file accessPattern.cpp
 * @author Juncheneg Yang (peter.waynechina@gmail.com)
 * @brief calculate the access pattern of the trace, and dump the result to file
 *
 * @version 0.1
 * @date 2023-06-16
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "accessPattern.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

namespace traceAnalyzer
{
    // 统计一个新请求
    void AccessPattern::add_req(const parser::Request *req)
    {
        // 请求数量过多
        if (n_seen_req_ > 0xfffffff0)
        {
            if (n_seen_req_ == 0xfffffff0)
            {
                INFO("trace is too long, accessPattern uses up to 0xfffffff0 requests\n");
            }
            return;
        }
        // 访问的请求数量+1
        n_seen_req_ += 1;

        // 若为设置起始时间，将起始时间赋值为当前请求的时间戳
        if (start_rtime_ == -1)
        {
            start_rtime_ = req->time;
        }

        // 不采样当前对象，跳过
        if (req->id % sample_ratio_ != 0)
        {
            /* skip this object */
            return;
        }
        // 记录当前对象的请求时间戳
        // 若对象id不在访问时间映射中，将其加入
        if (access_rtime_map_.find(req->id) == access_rtime_map_.end())
        {
            access_rtime_map_[req->id] = vector<uint32_t>();
            access_vtime_map_[req->id] = vector<uint32_t>();
        }
        // 相对于起始时间的实际时间戳
        access_rtime_map_[req->id].push_back(req->time - start_rtime_);
        // 逻辑时间戳(经过了多少个请求)
        access_vtime_map_[req->id].push_back(n_seen_req_);
    }

    // 按每个对象的第一次访问时间排序，将每个对象的所有访问时间戳按行输出到文件
    void AccessPattern::dump(string &path_base)
    {
        // 先输出实际时间戳到.accessRtime文件
        string ofile_path = path_base + ".accessRtime";
        ofstream ofs(ofile_path, ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        ofs << "# access pattern real time, each line stores all the real time of "
               "requests to an object\n";

        // sort the timestamp list by the first timestamp
        vector<vector<uint32_t> *> sorted_rtime_vec;
        // 将每个对象的访问时间戳数组指针加入到sorted_rtime_vec中
        for (auto &p : access_rtime_map_)
        {
            sorted_rtime_vec.push_back(&p.second);
        }
        // 按对象的第一次访问时间戳从小到大排序
        sort(sorted_rtime_vec.begin(), sorted_rtime_vec.end(),
             [](vector<uint32_t> *p1, vector<uint32_t> *p2)
             {
                 return (*p1).at(0) < (*p2).at(0);
             });

        // 遍历每个对象，并按行输出每个对象的访问时间戳
        for (const auto *p : sorted_rtime_vec)
        {
            for (const uint32_t rtime : *p)
            {
                ofs << rtime << ",";
            }
            ofs << "\n";
        }
        ofs << "\n"
            << endl;
        ofs.close();

        // 输出逻辑时间戳到.accessVtime文件
        ofs << "# " << path_base << "\n";
        string ofile_path2 = path_base + ".accessVtime";
        ofstream ofs2(ofile_path2, ios::out | ios::trunc);
        ofs2 << "# access pattern virtual time, each line stores all the virtual "
                "time of requests to an object\n";
        vector<vector<uint32_t> *> sorted_vtime_vec;
        for (auto &p : access_vtime_map_)
        {
            sorted_vtime_vec.push_back(&(p.second));
        }

        sort(sorted_vtime_vec.begin(), sorted_vtime_vec.end(),
             [](vector<uint32_t> *p1, vector<uint32_t> *p2) -> bool
             {
                 return (*p1).at(0) < (*p2).at(0);
             });

        for (const vector<uint32_t> *p : sorted_vtime_vec)
        {
            for (const uint32_t vtime : *p)
            {
                ofs2 << vtime << ",";
            }
            ofs2 << "\n";
        }

        ofs2.close();
    }

}; // namespace traceAnalyzer
