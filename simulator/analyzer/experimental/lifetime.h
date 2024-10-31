
#pragma once
/* plot the reuse distribution */

#include <unordered_map>
#include <vector>

#include "parsers/parser.hpp"
#include "analyzer/utils/utils.h"

namespace traceAnalyzer
{
    class LifetimeDistribution
    {
    public:
        LifetimeDistribution() = default;

        ~LifetimeDistribution() = default;

        void add_req(parser::Request *req);

        void dump(string &path_base);

    private:
        int64_t n_req_ = 0;
        // 记录对象的创建时间和最后一次访问时间(实际时间戳)
        unordered_map<int64_t, pair<int64_t, int64_t>> first_last_req_clock_time_map_;
        // 记录对象的创建时间和最后一次访问时间(逻辑时间戳，请求数)
        unordered_map<int64_t, pair<int64_t, int64_t>>
            first_last_req_logical_time_map_;

        const int64_t object_create_clock_time_max_ = 86400;
    };

} // namespace traceAnalyzer
