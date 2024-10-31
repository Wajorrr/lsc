#pragma once

#include <unordered_map>

#include "robin_hood.h"
#include "common/config.h"

typedef uint32_t obj_size_t;
// typedef int32_t time_t;

namespace traceAnalyzer
{
    struct obj_info
    {
        int64_t last_access_vtime; // 上一次访问的实际时间戳
        obj_size_t obj_size;       // 对象大小
        int32_t last_access_rtime; // 上一次访问的逻辑时间戳

        uint32_t freq;       // 访问频率
        uint32_t read_freq;  // 读取频率
        uint32_t write_freq; // 写入频率

        int32_t create_rtime; // 对象创建时间
        int32_t create_vtime; // 对象创建时间

        int32_t last_update_rtime; // 对象更新时间
        int32_t last_update_vtime; // 对象更新时间

    } __attribute__((packed));

    using obj_info_map_type =
        robin_hood::unordered_flat_map<obj_id_t, struct obj_info>;

} // namespace traceAnalyzer
