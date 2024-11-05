//
// Created by Juncheng Yang on 10/16/20.
//

#pragma once
#include <string>
#include <vector>

#include "common/logging.h"

using namespace std;

namespace utils
{

    // 用于向vector中插入元素
    template <typename T>
    void vector_insert(vector<T> &vec, int pos, T v)
    {
        // 超出容量，扩容2个单位
        if (pos >= vec.size())
        {
            /* resize the vector so that we can increase,
             * pos + 8 is to reduce the number of resize op */
            vec.resize(pos + 2, 0);
        }

        vec[pos] = v;
    }

    // 增加vector中的给定位置元素的值
    template <typename T>
    void vector_incr(vector<T> &vec, int pos, T v)
    {
        // 超出容量，扩容2个单位
        if (pos >= vec.size())
        {
            /* resize the vector so that we can increase,
             * pos + 8 is to reduce the number of resize op */
            vec.resize(pos + 2, 0);
        }

        vec[pos] += v;
    }

    // static inline void my_assert(bool b, const std::string &msg) {
    //   if (!b) {
    //     ERROR("%s", msg.c_str());
    //     abort();
    //   }
    // }
} // namespace utils
