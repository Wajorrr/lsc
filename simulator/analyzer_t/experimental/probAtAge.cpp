#include "probAtAge.h"

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
void ProbAtAge::add_req(parser::Request* req) {
    // 预热时间
    if (req->time < warmup_rtime_ || req->create_rtime < warmup_rtime_) {
        return;
    }
    // 重用时间包含多少个时间窗口
    int pos_access = (int)(req->rtime_since_last_access / time_window_);
    // 自对象创建以来的时间包含多少个时间窗口
    int pos_create = (int)((req->time - req->create_rtime) / time_window_);

    //<重用时间、创建以来的时间> 除以时间窗口大小，向下取整
    auto p = pair<int32_t, int32_t>(pos_access, pos_create);
    // 记录<重用时间、创建以来的时间>对应的请求次数，用于计算不同年龄的对象被重用的概率
    ac_age_req_cnt_[p] += 1;
}

void ProbAtAge::dump(string& path_base) {
    ofstream ofs2(path_base + ".probAtAge_w" + std::to_string(time_window_),
                  ios::out | ios::trunc);
    ofs2 << "# " << path_base << "\n";
    ofs2 << "# reuse access age, create age: req_cnt\n";

    // 遍历统计信息
    for (auto& p : ac_age_req_cnt_) {
        // 输出
        // 重用时间经过的时间窗口数、创建以来的时间经过的时间窗口数、对应的请求次数
        ofs2 << p.first.first * time_window_ << ","
             << p.first.second * time_window_ << ":" << p.second << "\n";
    }
}

};  // namespace traceAnalyzer
