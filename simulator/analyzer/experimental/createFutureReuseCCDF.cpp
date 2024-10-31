#include "createFutureReuseCCDF.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "analyzer/utils/struct.h"
#include "analyzer/utils/utils.h"

namespace traceAnalyzer
{
    using namespace std;

    void CreateFutureReuseDistribution::add_req(parser::Request *req)
    {
        // 预热时间
        if (req->time < warmup_time_)
        {
            warmup_obj_.insert(req->id);
            return;
        }

        // 若对象已经在预热时间内出现过，跳过
        if (warmup_obj_.find(req->id) != warmup_obj_.end())
        {
            return;
        }

        assert(req->next_access_vtime != -2);
        auto it = create_info_map_.find(req->id);

        // 若当前对象没出现过
        if (it == create_info_map_.end())
        {
            /* this is a new object */
            // 在预热时间内，跳过
            if (req->time > warmup_time_ + creation_time_window_)
            {
                return;
            }
            // 创建一个对象创建标识{创建时间、上一次读取时间、访问频率}
            create_info_map_[req->id] = {(int32_t)req->time, -1, 0};
            it = create_info_map_.find(req->id);
        }
        else
        {
            //      assert(req->next_access_vtime != -1);
            // printf("%ld %ld %ld %ld\n",
            //      req->time, req->id, it->second.create_rtime,
            //      it->second.freq);

            // 更新对象的上一次读取时间和访问频率
            create_info_map_[req->id] = {
                it->second.create_rtime, (int32_t)req->time, it->second.freq + 1};
        }

        // 自对象创建以来经过的时间
        int rtime_since_create = (int)req->time - it->second.create_rtime;
        assert(rtime_since_create >= 0);

        // 计算重用时间在分布中所处的位置
        int pos_rt = GET_POS_RT(rtime_since_create);
        assert(pos_rt < reuse_info_array_size_);

        if (req->next_access_vtime >= 0 && req->next_access_vtime != INT64_MAX)
        {                                       // 对象有未来请求
            reuse_info_[pos_rt].reuse_cnt += 1; // 重用次数+1

            // 重用频率累加，为了统计不同访问频率的对象的重用概率？
            reuse_info_[pos_rt].reuse_freq_sum += it->second.freq;
            // 计算访问年龄，即上一次读取时间到当前时间的时间差
            int32_t access_age =
                it->second.last_read_rtime == -1
                    ? 0
                    : (int32_t)req->time - it->second.last_read_rtime;
            // 访问年龄累加，为了统计不同访问年龄的对象的重用概率？
            reuse_info_[pos_rt].reuse_access_age_sum += access_age;
        }
        else // 若对象没有未来请求，这样的对象是驱逐的好候选者，单独统计
        {
            /* either no more requests, or the next request is write / delete */
            reuse_info_[pos_rt].stop_reuse_cnt += 1;
            reuse_info_[pos_rt].stop_reuse_freq_sum += it->second.freq;
            int32_t access_age =
                it->second.last_read_rtime == -1
                    ? 0
                    : (int32_t)req->time - it->second.last_read_rtime;
            reuse_info_[pos_rt].stop_reuse_access_age_sum += access_age;
            // do not erase because we want create time
            // create_info_map_.erase(req->id);
        }
    }

    void CreateFutureReuseDistribution::dump(string &path_base)
    {
        ofstream ofs(path_base + ".createFutureReuseCCDF", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        ofs << "# real time: reuse_cnt, stop_reuse_cnt, reuse_access_age_sum, "
               "stop_reuse_access_age_sum, "
            << "reuse_freq_sum, stop_reuse_freq_sum\n";

        // 遍历重用时间分布
        for (int idx = 0; idx < this->reuse_info_array_size_; idx++)
        {
            // 实际重用时间
            int ts = GET_RTIME_FROM_POS(idx);
            if (ts == -1)
                continue;
            if (this->reuse_info_[idx].reuse_cnt +
                    this->reuse_info_[idx].stop_reuse_cnt ==
                0) // 没有对象在该重用时间被访问过，跳过
                continue;

            // 输出重用时间、重用次数、最后一次重用次数、重用访问年龄总和、最后一次重用访问年龄总和、重用频率总和、最后一次重用频率总和
            ofs << ts << ":" << this->reuse_info_[idx].reuse_cnt << ","
                << this->reuse_info_[idx].stop_reuse_cnt << ","
                << this->reuse_info_[idx].reuse_access_age_sum << ","
                << this->reuse_info_[idx].stop_reuse_access_age_sum << ","
                << this->reuse_info_[idx].reuse_freq_sum << ","
                << this->reuse_info_[idx].stop_reuse_freq_sum << "\n";
        }
        // ofs << "-1: " << this->create_info_map_.size() << "\n";
    }

}; // namespace traceAnalyzer
