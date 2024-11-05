#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "parsers/parser.hpp"

using namespace std;

namespace traceAnalyzer {

class OpStat {
   public:
    OpStat() = default;
    ~OpStat() = default;

    // 统计一个新的请求的操作类型
    inline void add_req(parser::Request* req) {
        // 请求的操作类型出现次数+1
        op_cnt_[req->type] += 1;
        if (req->overwrite)  // 是否覆写了一个之前的对象
            overwrite_cnt_ += 1;
    }

    friend ostream& operator<<(ostream& os, const OpStat& op) {
        // 用于构造输出字符串
        stringstream stat_ss;

        // 总请求数
        uint64_t n_req =
            accumulate(op.op_cnt_, op.op_cnt_ + parser::OP_INVALID + 1, 0UL);
        // 写请求次数
        uint64_t n_write = op.op_cnt_[parser::OP_SET] +
                           op.op_cnt_[parser::OP_REPLACE] +
                           op.op_cnt_[parser::OP_CAS];
        // 删除请求次数
        uint64_t n_del = op.op_cnt_[parser::OP_DELETE];
        // 覆写请求次数
        uint64_t n_overwrite = op.overwrite_cnt_;

        // 若无效操作次数小于总请求数的一半
        if (op.op_cnt_[parser::OP_INVALID] < n_req / 2) {
            stat_ss << fixed << setprecision(4) << "op: ";
            // 输出每种操作类型的出现次数和占比
            for (int i = 0; i < parser::OP_INVALID + 1; i++) {
                stat_ss << parser::req_op_str[i] << ":" << op.op_cnt_[i] << "("
                        << (double)op.op_cnt_[i] / (double)n_req << "), ";
            }
            stat_ss << "\n";
        }
        // 输出写、覆写和删除请求次数及占比
        stat_ss << "write: " << n_write << "("
                << (double)n_write / (double)n_req << "), "
                << "overwrite: " << n_overwrite << "("
                << (double)n_overwrite / (double)n_req << "), "
                << "del:" << n_del << "(" << (double)n_del / (double)n_req
                << ")\n";
        // 将统计结果字符串写入到输出流
        os << stat_ss.str();

        return os;
    }

   private:
    // 不同操作类型的出现次数
    uint64_t op_cnt_[parser::OP_INVALID + 1] = {
        0}; /* the number of requests of an op */
    uint64_t overwrite_cnt_ = 0;
};
}  // namespace traceAnalyzer