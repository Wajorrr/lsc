
#pragma once

#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "analyzer_utils/struct.h"
#include "common/macro.h"
#include "parsers/parser.hpp"

namespace traceAnalyzer {
class SizeDistribution {
    /**
     * track a few things
     * 1. obj_size request count
     * 2. obj_size object  count
     * 3. obj_size request count over time (heatmap plot) // 取对数
     * 4. obj_size object  count over time (heatmap plot) // 取对数
     */
   public:
    SizeDistribution() = default;
    explicit SizeDistribution(std::string& output_path, int time_window)
        : time_window_(time_window) {
        obj_size_req_cnt_.reserve(1e6);
        obj_size_obj_cnt_.reserve(1e6);

        turn_on_stream_dump(output_path);
    };

    ~SizeDistribution() {
        ofs_stream_req.close();
        ofs_stream_obj.close();
    }

    void add_req(parser::Request* req);

    void dump(std::string& path_base);

   private:
    /* request/object count of certain size, size->count */
    std::unordered_map<obj_size_t, uint32_t>
        obj_size_req_cnt_;  // 请求的对象大小分布
    std::unordered_map<obj_size_t, uint32_t>
        obj_size_obj_cnt_;  // 第一次出现的对象的大小分布

    /* used to plot size distribution heatmap */
    const double LOG_BASE = 1.5;
    const double log_log_base = log(LOG_BASE);
    const int time_window_ = 300;
    int64_t next_window_ts_ = -1;

    std::vector<uint32_t> window_obj_size_req_cnt_;  // 请求的对象的对数大小分布
    std::vector<uint32_t>
        window_obj_size_obj_cnt_;  // 第一次出现的对象的对数大小分布

    // 输出文件流
    std::ofstream ofs_stream_req;
    std::ofstream ofs_stream_obj;

    void turn_on_stream_dump(std::string& path_base);

    void stream_dump();
};

}  // namespace traceAnalyzer