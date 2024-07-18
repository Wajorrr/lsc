#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <stdint.h>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../constants.hpp"
#include "../lib/zipf.h"

typedef double double64_t;

namespace parser
{

  class ZipfParser : public virtual Parser
  {

  public:
    ZipfParser(float alpha, uint64_t numObjects, uint64_t _numRequests)
        : zipf("", numObjects, alpha, 1), numRequests(_numRequests)
    {
    }

    void go(VisitorFn visit) // VisitorFn visit为缓存的访问函数
    {
      std::cout << "go: Generating "
                << numRequests << " requests\n";
      Request req;
      req.type = parser::OP_GET;
      uint64_t obj_id;
      uint64_t obj_size;
      for (uint64_t i = 0; i < numRequests; i++)
      {
        zipf.Sample(obj_id, obj_size); // 从zipf分布中采样一个对象，返回对象ID和对象大小
        req.id = obj_id;               // 请求id赋值为对象id
        req.req_size = obj_size;       // 请求大小赋值为对象大小
        req.req_num = i;               // 请求数量
        visit(&req);                   // 使缓存处理当前请求
      }
    }

  private:
    ZipfRequests zipf;
    uint64_t numRequests;
  };

} // namespace parser
