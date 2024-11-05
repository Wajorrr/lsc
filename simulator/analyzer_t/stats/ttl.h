#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/logging.h"
#include "parsers/parser.hpp"

namespace traceAnalyzer
{

    class TtlStat
    {
    public:
        TtlStat() = default;
        ~TtlStat() = default;

        // 将新请求的ttl值加入到ttl_cnt_统计中
        void add_req(parser::Request *req);

        // 将ttl的统计结果写入到输出流
        friend std::ostream &operator<<(std::ostream &os, const TtlStat &ttl)
        {
            // 用于构造要输出的字符串
            std::stringstream stat_ss;

            // 有多少不同ttl
            std::cout << "TTL: " << ttl.ttl_cnt_.size() << " different TTLs, ";
            // 总ttl数/总请求数
            uint64_t n_req = std::accumulate(
                std::begin(ttl.ttl_cnt_), std::end(ttl.ttl_cnt_), 0ULL,
                [](uint64_t value,
                   const std::unordered_map<int32_t, uint32_t>::value_type &p)
                {
                    return value + p.second;
                });

            // 若有超过1种不同的ttl
            if (ttl.ttl_cnt_.size() > 1)
            {
                stat_ss << "TTL: " << ttl.ttl_cnt_.size() << " different TTLs, ";
                // 遍历不同ttl，分别输出ttl值、出现次数和出现次数占比
                for (auto it : ttl.ttl_cnt_)
                {
                    if (it.second > (size_t)((double)n_req * 0.01))
                    {
                        stat_ss << it.first << ":" << it.second << "("
                                << (double)it.second / (double)n_req << "), ";
                    }
                }
                stat_ss << "\n";
            }
            // 将统计结果字符串写入到输出流
            os << stat_ss.str();

            return os;
        }

        // 将ttl的统计结果写入文件
        void dump(const std::string &filename);

    private:
        /* the number of requests have ttl value */
        std::unordered_map<int32_t, uint32_t> ttl_cnt_{}; // 不同ttl值的出现次数
        // 不同ttl值的总数是否太多
        bool too_many_ttl_ = false;
    };
} // namespace traceAnalyzer