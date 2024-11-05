
#include "ttl.h"

#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace traceAnalyzer
{
    using namespace std;

    void TtlStat::add_req(parser::Request *req)
    {
        // printf("req->ttl: %d\n", req->ttl);

        if (req->ttl > 0)
        {
            // 当前ttl的出现次数
            auto it = ttl_cnt_.find(req->ttl);
            // 当前ttl没出现过
            if (it == ttl_cnt_.end())
            {
                // 插入新的ttl
                ttl_cnt_[req->ttl] = 1;
                // 当前trace中不同ttl的总数大于1000000
                if ((!too_many_ttl_) && ttl_cnt_.size() > 1000000)
                {
                    too_many_ttl_ = true;
                    WARN("there are too many TTLs (%zu) in the trace\n", ttl_cnt_.size());
                }
            }
            else // 当前ttl出现过，出现次数+1
            {
                it->second += 1;
            }
        }
    }

    // 将ttl的统计结果写入文件
    void TtlStat::dump(const string &path_base)
    {
        ofstream ofs(path_base + ".ttl", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        // 如果不同ttl的总数太多，直接返回
        if (too_many_ttl_)
        {
            ofs << "# there are too many TTLs in the trace\n";
            return;
        }
        else // 否则，将不同ttl的出现次数写入文件
        {
            ofs << "# TTL: req_cnt\n";
            for (auto &p : ttl_cnt_)
            {
                ofs << p.first << ":" << p.second << "\n";
            }
        }
        ofs.close();
    }
}; // namespace traceAnalyzer
