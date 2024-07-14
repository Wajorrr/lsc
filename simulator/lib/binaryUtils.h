#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include "../common/logging.h"
#include "../parsers/binary_parser.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    static inline int format_to_size(char format)
    {
        switch (format)
        {
        case 'c':
        case 'b':
        case 'B':
            return 1;
        case 'h':
        case 'H':
            return 2;
        case 'i':
        case 'I':
        case 'l':
        case 'L':
        case 'f':
            return 4;
        case 'q':
        case 'Q':
        case 'd':
            return 8;
        default:
            ERROR("unknown format '%c'\n", format);
        }
    }

    static int cal_offset(const char *format_str, int field_idx)
    {
        int offset = 0;
        for (int i = 0; i < field_idx - 1; i++)
        {
            offset += format_to_size(format_str[i]);
        }
        return offset;
    }

    static inline int64_t read_data(char *src, char format)
    {
        // 根据不同的类型读取相应长度的数据
        switch (format)
        {
        case 'b':
        case 'B':
        case 'c':
            return (int64_t)(*(int8_t *)src);
        case 'h':
        case 'H':
            return (int64_t)(*(int16_t *)src);
        case 'i':
        case 'l':
        case 'I':
        case 'L':
            return (int64_t)(*(int32_t *)src);
        case 'q':
        case 'Q':
            return (int64_t)(*(int64_t *)src);
        case 'f':
            return (int64_t)(*(float *)src);
        case 'd':
            return (int64_t)(*(double *)src);
        default:
            ERROR("DO NOT recognize given format character: %c\n", format);
            break;
        }
    }

#ifdef __cplusplus
}
#endif
