#pragma once
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include "../constants.hpp"
#include "../lib/zipf.h"
#include "common/logging.h"

typedef double double64_t;

namespace parser
{

    class ZipfParser : public virtual Parser
    {
    public:
        // 参数：alpha为zipf分布的参数，numObjects为对象数量，_numRequests为请求数量
        ZipfParser(float alpha, uint64_t numObjects, uint64_t _numRequests, float writeRatio)
            : zipf("", numObjects, alpha, 1), numRequests(_numRequests), writeRatio(writeRatio)
        {
        }

        void go(VisitorFn visit) // VisitorFn visit为缓存的访问函数
        {
            std::cout << "go: Generating "
                      << numRequests << " requests\n";
            std::cout << "writeRatio: " << writeRatio << std::endl;
            Request req;
            req.type = parser::OP_GET;
            uint64_t obj_id;
            uint64_t obj_size;

            // 随机数生成器相关成员变量
            std::random_device rd;                          // 用于生成种子
            std::mt19937 gen;                               // 使用 Mersenne Twister 算法的随机数生成器
            std::uniform_real_distribution<> dis(0.0, 1.0); // 生成 0 到 1 之间的均匀分布的随机数

            for (uint64_t i = 0; i < numRequests; i++)
            {
                zipf.Sample(obj_id, obj_size); // 从zipf分布中采样一个对象，返回对象ID和对象大小
                req.id = obj_id;               // 请求id赋值为对象id
                req.req_size = obj_size;       // 请求大小赋值为对象大小
                req.req_num = i;               // 请求数量

                // 根据 writeRatio 随机设置 req.type
                if (dis(gen) < writeRatio)
                {
                    req.type = parser::OP_SET;
                }
                else
                {
                    req.type = parser::OP_GET;
                }

                // DEBUG("req.id:%d,req.req_size:%d,req.req_num:%d\n", req.id, req.req_size, req.req_num);

                visit(&req); // 使缓存处理当前请求
            }
        }

        int read_one_req(parser::Request *req)
        {
            uint64_t obj_id;
            uint64_t obj_size;
            zipf.Sample(obj_id, obj_size); // 从zipf分布中采样一个对象，返回对象ID和对象大小
            req->id = obj_id;              // 请求id赋值为对象id
            req->req_size = obj_size;      // 请求大小赋值为对象大小
            // req->req_num = i;              // 请求数量

            std::mt19937 gen;                               // 使用 Mersenne Twister 算法的随机数生成器
            std::uniform_real_distribution<> dis(0.0, 1.0); // 生成 0 到 1 之间的均匀分布的随机数

            // 根据 writeRatio 随机设置 req->type
            if (dis(gen) < writeRatio)
            {
                req->type = parser::OP_SET;
            }
            else
            {
                req->type = parser::OP_GET;
            }
        }

    private:
        ZipfRequests zipf;
        uint64_t numRequests;
        float writeRatio;
    };

} // namespace parser
