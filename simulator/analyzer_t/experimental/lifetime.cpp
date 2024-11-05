
#include "lifetime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace traceAnalyzer {
using namespace std;

void LifetimeDistribution::add_req(parser::Request* req) {
    n_req_++;
    // printf("%ld\n", req->time);
    auto itr = first_last_req_clock_time_map_.find(req->id);
    // 该对象没出现过
    if (itr == first_last_req_clock_time_map_.end()) {
        // 对象创建时间小于统计阈值
        if (req->time < object_create_clock_time_max_) {
            uint64_t time = req->time;
            // 记录对象的创建时间和最后一次访问时间(实际时间戳)
            first_last_req_clock_time_map_[req->id] =
                pair<int64_t, int64_t>(time, time);
            // 记录对象的创建时间和最后一次访问时间(逻辑时间戳，请求数)
            first_last_req_logical_time_map_[req->id] =
                pair<int64_t, int64_t>(n_req_, n_req_);
        }
    } else {
        // 更新对象的最后一次访问的实际时间戳和逻辑时间戳
        itr->second.second = req->time;
        auto itr2 = first_last_req_logical_time_map_.find(req->id);
        itr2->second.second = n_req_;
    }
}

void LifetimeDistribution::dump(string& path_base) {
    unordered_map<int64_t, int64_t> clock_lifetime_distribution;
    // 遍历对象生命周期的实际时间戳
    for (auto& p : first_last_req_clock_time_map_) {
        // 对象生命周期，先除再乘，但这样0～9的生命周期会被归为0，一次访问对象的数量统计不准确
        // int64_t lifetime = ((int64_t)(p.second.second - p.second.first) / 10)
        // * 60;

        int64_t lifetime;
        if (p.second.second - p.second.first >= 10)
            lifetime = ((int64_t)(p.second.second - p.second.first) / 10) * 10;
        else
            lifetime = p.second.second - p.second.first;

        // 对对象生命周期的分布进行统计
        auto itr = clock_lifetime_distribution.find(lifetime);
        if (itr == clock_lifetime_distribution.end()) {
            clock_lifetime_distribution[lifetime] = 1;
        } else {
            itr->second++;
        }
    }

    // 对对象生命周期的分布按生命周期长度从小到大进行排序
    vector<pair<int64_t, int64_t>> clock_lifetime_distribution_vec;
    for (auto& p : clock_lifetime_distribution) {
        clock_lifetime_distribution_vec.push_back(p);
    }
    sort(clock_lifetime_distribution_vec.begin(),
         clock_lifetime_distribution_vec.end(),
         [](const pair<int64_t, int64_t>& a, const pair<int64_t, int64_t>& b) {
             return a.first < b.first;
         });

    ofstream ofs(path_base + ".lifetime", ios::out | ios::trunc);
    ofs << "# " << path_base << "\n";
    ofs << "# lifetime distribution (clock) \n";
    // 输出对象生命周期的分布 <生命周期，对象个数>
    for (auto& p : clock_lifetime_distribution_vec) {
        ofs << p.first << ":" << p.second << "\n";
    }

    // 对逻辑时间戳，再进行一次统计和输出
    unordered_map<int64_t, int64_t> logical_lifetime_distribution;
    for (auto& p : first_last_req_logical_time_map_) {
        int64_t lifetime =
            ((int64_t)(p.second.second - p.second.first) / 100) * 100;
        auto itr = logical_lifetime_distribution.find(lifetime);
        if (itr == logical_lifetime_distribution.end()) {
            logical_lifetime_distribution[lifetime] = 1;
        } else {
            itr->second++;
        }
    }
    vector<pair<int64_t, int64_t>> logical_lifetime_distribution_vec;
    for (auto& p : logical_lifetime_distribution) {
        logical_lifetime_distribution_vec.push_back(p);
    }
    sort(logical_lifetime_distribution_vec.begin(),
         logical_lifetime_distribution_vec.end(),
         [](const pair<int64_t, int64_t>& a, const pair<int64_t, int64_t>& b) {
             return a.first < b.first;
         });

    ofs << "# lifetime distribution (logical) \n";
    for (auto& p : logical_lifetime_distribution_vec) {
        ofs << p.first << ":" << p.second << "\n";
    }
    ofs.close();
}
};  // namespace traceAnalyzer
