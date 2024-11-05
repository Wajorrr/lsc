
#pragma once
/* plot the reuse distribution */

#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "analyzer_t/utils/struct.h"
#include "analyzer_t/utils/utils.h"
#include "parsers/parser.hpp"

namespace traceAnalyzer {
class ReuseDistribution {
   public:
    explicit ReuseDistribution(std::string output_path,
                               int time_window = 300,
                               int rtime_granularity = 5,
                               int vtime_granularity = 1000)
        : time_window_(time_window),
          rtime_granularity_(rtime_granularity),
          vtime_granularity_(vtime_granularity) {
        turn_on_stream_dump(output_path);
    };

    ~ReuseDistribution() {
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

    std::ofstream stream_dump_rt_ofs;  // 实际重用时间间隔分布的输出文件流
    std::ofstream stream_dump_vt_ofs;  // 逻辑重用时间间隔分布的输出文件流

    void turn_on_stream_dump(std::string& path_base);

    void stream_dump_window_reuse_distribution();
};

}  // namespace traceAnalyzer
