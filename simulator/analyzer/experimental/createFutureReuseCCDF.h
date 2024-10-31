
#pragma once
/* calculate the reuse CCDF at certain create age, each point (x, y) in the
 * distribution measures the probability of having future reuse y at age x */

#include <assert.h>
#include <unordered_map>
#include <unordered_set>

#include "parsers/parser.hpp"
#include "analyzer/utils/struct.h"
#include "analyzer/utils/utils.h"

namespace traceAnalyzer
{

    class CreateFutureReuseDistribution
    {
    public:
        explicit CreateFutureReuseDistribution(int warmup_time = 7200)
            : warmup_time_(warmup_time)
        {
            assert(reuse_info_array_size_ ==
                   max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_);
            reuse_info_ = new create_reuse_info_t[reuse_info_array_size_];
            memset(reuse_info_, 0,
                   sizeof(create_reuse_info_t) * reuse_info_array_size_);
        }

        ~CreateFutureReuseDistribution() { delete[] reuse_info_; }

        // 计算重用时间的值对应的偏移量(用于更好地表示分布)
        inline int GET_POS_RT(int t)
        {
            if (t > fine_rtime_upbound_)
                return (int)(t / rtime_granularity_) + fine_rtime_upbound_;
            else
                return t;
        }

        // 获取重用时间分布中当前位置对应的实际重用时间
        inline int GET_RTIME_FROM_POS(int p)
        {
            if (p > fine_rtime_upbound_)
            {
                /* this is not a valid pos, because there is a gap between fine_rtime and
                 * coarse_rtime */
                if (p < fine_rtime_upbound_ + fine_rtime_upbound_ / rtime_granularity_)
                    return -1;
                else
                    return (p - fine_rtime_upbound_) * rtime_granularity_;
            }
            else
            {
                return p;
            }
        }

        void add_req(parser::Request *req);

        void dump(string &path_base);

    private:
        struct create_info
        {
            int32_t create_rtime;    // 对象创建时间
            int32_t last_read_rtime; // 上一次读取时间
            int32_t freq;            // 访问频率(次数)
        } __attribute__((packed));

        /* obj -> (write_time, read_time), read time is used to check it has been read
         */
        //  unordered_map<obj_id_t, struct create_info> create_info_map_;
        robin_hood::unordered_flat_map<obj_id_t, struct create_info> create_info_map_;

        /* future reuse (whether it has future request) in the struct */
        typedef struct
        {
            uint32_t reuse_cnt;                 // 该重用时间的正常重用次数(不计最后一次重用)
            uint32_t stop_reuse_cnt;            // 该重用时间的最后一次重用次数(最后一次重用的对象是好的驱逐候选者)
            uint64_t reuse_access_age_sum;      // 该重用时间的正常重用的访问年龄之和(访问年龄为对象从创建到访问经过的时间)
            uint64_t reuse_freq_sum;            // 该重用时间的正常重用的访问频率之和(访问频率为对象的访问次数)
            uint64_t stop_reuse_access_age_sum; // 该重用时间的最后一次重用的访问年龄之和
            uint64_t stop_reuse_freq_sum;       // 该重用时间的最后一次重用的访问频率之和
        } create_reuse_info_t;

        /* the request cnt that get a reuse or stop reuse at create age */
        create_reuse_info_t *reuse_info_;

        const int warmup_time_ = 86400;
        unordered_set<obj_id_t> warmup_obj_;

        /* we only consider objects created between warmup_time_ +
         * creation_time_window_ to avoid the impact of trace ending */
        const int creation_time_window_ = 86400;

        /* rtime smaller than fine_rtime_upbound_ will not be divide by
         * rtime_granularity_ */
        const int fine_rtime_upbound_ = 3600; // 重用时间分布下界，小于该值的重用时间不会被rtime_granularity_划分
        const int rtime_granularity_ = 5;     // 重用时间分布的划分粒度

        const int max_reuse_rtime_ = 86400 * 80;
        const int reuse_info_array_size_ =
            max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_; // 重用时间分布的上界(最大重用时间所处的位置)
    };

} // namespace traceAnalyzer
