
#pragma once
/* plot the reuse distribution */

#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "analyzer_utils/struct.h"
#include "common/macro.h"
#include "parsers/parser.hpp"
// #include "utils/utils.h"

namespace traceAnalyzer {
class LifespanDistribution {
   public:
    explicit LifespanDistribution(std::string output_path,
                                  int time_window = 300,
                                  int rtime_granularity = 5,
                                  int vtime_granularity = 1000)
        : time_window_(time_window),
          rtime_granularity_(rtime_granularity),
          vtime_granularity_(vtime_granularity) {
        turn_on_stream_dump(output_path);
    };

    ~LifespanDistribution() {
        stream_dump_rt_ofs.close();
        stream_dump_vt_ofs.close();
    }

    void add_req(parser::Request* req);

    void dump(std::string& path_base);

   private:
    /* request count for reuse rtime/vtime */
    std::unordered_map<int32_t, uint32_t>
        reuse_rtime_req_cnt_;  // 总体实际重用时间间隔分布
    std::unordered_map<int32_t, uint32_t>
        reuse_vtime_req_cnt_;  // 总体逻辑重用时间间隔分布

    std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_read_;
    std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_write_;
    std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_delete_;

    /* used to plot reuse distribution heatmap */
    const double log_base_ = 1.5;
    const double log_log_base_ = log(log_base_);
    const int rtime_granularity_;
    const int vtime_granularity_;
    const int time_window_;
    int64_t next_window_ts_ = -1;

    std::vector<uint32_t>
        window_reuse_rtime_req_cnt_;  // 时间窗口内的实际重用时间间隔分布
    std::vector<uint32_t>
        window_reuse_vtime_req_cnt_;  // 时间窗口内的逻辑重用时间间隔分布

    // 若为写操作，当前时间减去上一次更新时间
    std::vector<uint32_t>
        window_lifetime_rtime_req_cnt_;  // 时间窗口内的生命周期分布(实际时间)
    std::vector<uint32_t>
        window_lifetime_vtime_req_cnt_;  // 时间窗口内的生命周期分布(逻辑时间)

    // 全局信息，应当在obj_info中统计
    std::unordered_map<uint32_t, uint32_t> read_cnt;  // 对象的读请求次数分布
    std::unordered_map<uint32_t, uint32_t> write_cnt;  // 对象的写请求次数分布

    // 首先判断是否在缓存中
    std::unordered_map<uint32_t, uint32_t>
        rlifespan_readprob;  // <生命周期，在该生命周期(创建以来)被读取的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        rlifespan_writeprob;  // <生命周期，在该生命周期(创建以来)被写入的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        rlifespan2_readprob;  // <生命周期，在该生命周期(自上次更新)被读取的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        rlifespan2_writeprob;  // <生命周期，在该生命周期(自上次更新)被写入的对象个数>

    std::unordered_map<uint32_t, uint32_t>
        vlifespan_readprob;  // <生命周期，在该生命周期(创建以来)被读取的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        vlifespan_writeprob;  // <生命周期，在该生命周期(创建以来)被写入的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        vlifespan2_readprob;  // <生命周期，在该生命周期(自上次更新)被读取的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        vlifespan2_writeprob;  // <生命周期，在该生命周期(自上次更新)被写入的对象个数>

    std::unordered_map<uint32_t, uint32_t>
        rlifespan_deleteprob;  // <生命周期，在该生命周期(创建以来)被删除的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        vlifespan_deleteprob;  // <生命周期，在该生命周期(创建以来)被删除的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        rlifespan2_deleteprob;  // <生命周期，在该生命周期(自上次更新)被删除的对象个数>
    std::unordered_map<uint32_t, uint32_t>
        vlifespan2_deleteprob;  // <生命周期，在该生命周期(自上次更新)被删除的对象个数>

    std::ofstream stream_dump_rt_ofs;  // 实际重用时间间隔分布的输出文件流
    std::ofstream stream_dump_vt_ofs;  // 逻辑重用时间间隔分布的输出文件流

    void turn_on_stream_dump(std::string& path_base);

    void stream_dump_window_reuse_distribution();
};

}  // namespace traceAnalyzer
