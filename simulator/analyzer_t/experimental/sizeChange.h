#pragma once
/**
 * size of the same object may change over time,
 * this file calculates the distribution of size change
 *
 */

#include <assert.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "analyzer_t/utils/robin_hood.h"
#include "analyzer_t/utils/utils.h"
#include "parsers/parser.hpp"

namespace traceAnalyzer {

class SizeChangeDistribution {
   public:
    SizeChangeDistribution() = default;

    // 根据对象大小变化比例返回对应的桶序号
    inline int relative_change_to_array_pos(double relative_change) {
        // 中间位置，大于中间位置表示增大，小于中间位置表示减小
        static int mid_pos = n_bins_relative / 2;

        // 大小变化比例小于一个阈值，则返回中间位置的桶
        if (relative_change < 0.00001 && relative_change > -0.00001)
            return mid_pos;

        if (relative_change > 0)  // 增大
        {
            if (relative_change < 0.0001)
                return mid_pos - 1;
            else if (relative_change < 0.0002)
                return mid_pos - 2;
            else if (relative_change < 0.0005)
                return mid_pos - 3;
            else if (relative_change < 0.0008)
                return mid_pos - 4;
            else if (relative_change < 0.001)
                return mid_pos - 5;
            else if (relative_change < 0.002)
                return mid_pos - 6;
            else if (relative_change < 0.005)
                return mid_pos - 7;
            else if (relative_change < 0.008)
                return mid_pos - 8;
            else if (relative_change < 0.01)
                return mid_pos - 9;
            else if (relative_change < 0.02)
                return mid_pos - 10;
            else if (relative_change < 0.05)
                return mid_pos - 11;
            else if (relative_change < 0.08)
                return mid_pos - 12;
            else if (relative_change < 0.1)
                return mid_pos - 13;
            else
                return mid_pos - 14;
        } else  // 减小
        {
            if (relative_change < 0.0001)
                return mid_pos + 1;
            else if (relative_change < 0.0002)
                return mid_pos + 2;
            else if (relative_change < 0.0005)
                return mid_pos + 3;
            else if (relative_change < 0.0008)
                return mid_pos + 4;
            else if (relative_change < 0.001)
                return mid_pos + 5;
            else if (relative_change < 0.002)
                return mid_pos + 6;
            else if (relative_change < 0.005)
                return mid_pos + 7;
            else if (relative_change < 0.008)
                return mid_pos + 8;
            else if (relative_change < 0.01)
                return mid_pos + 9;
            else if (relative_change < 0.02)
                return mid_pos + 10;
            else if (relative_change < 0.05)
                return mid_pos + 11;
            else if (relative_change < 0.08)
                return mid_pos + 12;
            else if (relative_change < 0.1)
                return mid_pos + 13;
            else
                return mid_pos + 14;
        }
    }

    ~SizeChangeDistribution() = default;

    // 根据对象大小变化差值返回对应的桶序号
    int absolute_change_to_array_pos(int absolute_change);

    void add_req(parser::Request* req);

    friend std::ostream& operator<<(std::ostream& os,
                                    const SizeChangeDistribution& size_change) {
        // 用于构造输出字符串
        std::stringstream stat_ss;

        // 请求总数
        uint64_t n_req = accumulate(
            size_change.absolute_size_change_cnt_,
            size_change.absolute_size_change_cnt_ + n_bins_absolute, 0UL);
        uint64_t n_req2 = accumulate(
            size_change.relative_size_change_cnt_,
            size_change.relative_size_change_cnt_ + n_bins_relative, 0UL);
        assert(n_req == n_req2);

        // 采样比例？但是好像没进行采样啊
        if ((double)n_req / (double)size_change.n_req_total_ < 0.001)
            return os;

        stat_ss << "relative size change: " << fixed;
        // 输出相对大小变化的分布情况
        for (int i = 0; i < n_bins_relative; i++) {
            uint64_t cnt = size_change.relative_size_change_cnt_[i];
            // 输出桶的标签(大小变化比例)和对应的请求数
            if (cnt > 0)
                stat_ss << size_change.relative_size_change_labels[i] << ":"
                        << cnt << "(" << (double)cnt / (double)n_req << "), ";
        }
        stat_ss << "\n";

        stat_ss << "absolute size change: " << fixed;
        // 输出绝对大小变化的分布情况
        for (int i = 0; i < n_bins_absolute; i++) {
            uint64_t cnt = size_change.absolute_size_change_cnt_[i];
            // 输出桶的标签(大小变化差值)和对应的请求数
            if (cnt > 0)
                stat_ss << size_change.absolute_size_change_labels[i] << ":"
                        << cnt << "(" << (double)cnt / (double)n_req << "), ";
        }
        stat_ss << "\n";

        // 将字符串流中的统计结果输出到输出流
        os << stat_ss.str();

        return os;
    }

    int64_t n_req_total_ = 0;

   private:
    // 按范围来统计对象大小变化的分布情况，分成一个个的bin，每个bin一个区间

    /* bins 0%, 0.01%, 0.02%, 0.05%, 0.08%, 0.1%, 0.2%, 0.5%,
     * 0.8%, 1.0%, 2.0%, 5.0%, 8.0%, 10.0%, >10% ... */

    // 对象大小变化的相对比例，使用29个桶来进行分布情况统计
    static const int n_bins_relative = 14 * 2 + 1;
    uint64_t relative_size_change_cnt_[n_bins_relative] = {
        0}; /* the number of requests of an op */
    std::string relative_size_change_labels[n_bins_relative] = {
        "<-0.1",   "-0.1",    "-0.08",  "-0.05",  "-0.02",   "-0.01",
        "-0.008",  "-0.005",  "-0.002", "-0.001", "-0.0008", "-0.0005",
        "-0.0002", "-0.0001", "0",      "0.0001", "0.0002",  "0.0005",
        "0.0008",  "0.001",   "0.002",  "0.005",  "0.008",   "0.01",
        "0.02",    "0.05",    "0.08",   "0.1",    "<0.1"};

    /* bins 0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, .. 2**14,
     * >2**14
     */

    // 对象大小变化的绝对差值，使用31个桶来进行分布情况统计
    static const int n_bins_absolute = 16 * 2 + 1;
    uint64_t absolute_size_change_cnt_[n_bins_absolute] = {0};
    std::string absolute_size_change_labels[n_bins_absolute] = {
        "<-16384", "-16384", "-8192", "-4096", "-2048", "-1024", "-512",
        "-256",    "-128",   "-64",   "-32",   "-16",   "-8",    "-4",
        "-2",      "-1",     "0",     "1",     "2",     "4",     "8",
        "16",      "32",     "64",    "128",   "256",   "512",   "1024",
        "2048",    "4096",   "8192",  "16384", ">16384"};
};
}  // namespace traceAnalyzer