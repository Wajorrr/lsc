
#include "sizeChange.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace traceAnalyzer {

using namespace std;

int SizeChangeDistribution::absolute_change_to_array_pos(int absolute_change) {
    // 中间位置，大于中间位置表示增大，小于中间位置表示减小
    static int mid_pos = n_bins_absolute / 2;
    // 中间位置表示大小没有变化
    if (absolute_change == 0)
        return mid_pos;

    int pos = mid_pos;

    if (absolute_change > 0)  // 增大
    {
        unsigned change = absolute_change;
        // 桶之间大小按2的指数增长
        // 找到大小变化的差值所对应的桶
        while (change > 0) {
            change = change >> 1u;
            pos += 1;
        }
    } else  // 减小
    {
        // 转成正数，以便于计算位置
        unsigned change = -absolute_change;
        // 找到大小变化的差值所对应的桶
        while (change > 0) {
            change = change >> 1u;
            pos -= 1;
        }
    }

    if (pos >= n_bins_absolute)  // 上界
        pos = n_bins_absolute - 1;
    else if (pos < 0)  // 下界
        pos = 0;

    return pos;  // 返回对应的桶序号
}

void SizeChangeDistribution::add_req(parser::Request* req) {
    n_req_total_ += 1;
    if (req->overwrite)  // 覆写请求
    {
        // 覆写之后对象的大小变化差值
        int absolute_size_change = (int)req->req_size - (int)req->prev_size;
        // 对象大小变化的比例
        double relative_size_change =
            (double)absolute_size_change / (double)req->prev_size;
        // 对象大小变化的差值的分布情况统计
        absolute_size_change_cnt_[absolute_change_to_array_pos(
            absolute_size_change)]++;
        // 对象大小变化的比例的分布情况统计
        relative_size_change_cnt_[relative_change_to_array_pos(
            relative_size_change)]++;

        //      if (absolute_size_change > 4096) {
        //        print_request(req);
        //        printf("%lf\n", relative_size_change);
        //        printf("%ld %ld %d %ld\n", req->prev_size, req->req_size,
        //               relative_change_to_array_pos(relative_size_change),
        //               relative_size_change_cnt_[relative_change_to_array_pos(relative_size_change)]);
        //      }
    }
    //    if (req->obj_id == 3102)
    //      print_request(req);
}

};  // namespace traceAnalyzer
