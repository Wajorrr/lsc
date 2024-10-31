#pragma once

#include <libconfig.h++>
#include <cstring>
#include "common/mem.h"

#define SIZE_OF_KEY 20 + 24 // bytes
#define MAX_TAO_SIZE 2048

namespace parser
{

  typedef enum
  {
    OP_NOP = 0, // 无操作
    OP_GET = 1, // 读取操作
    OP_GETS = 2,
    OP_SET = 3,
    OP_ADD = 4,
    OP_CAS = 5,
    OP_REPLACE = 6,
    OP_APPEND = 7,
    OP_PREPEND = 8,
    OP_DELETE = 9,
    OP_INCR = 10,
    OP_DECR = 11,

    OP_READ,
    OP_WRITE,
    OP_UPDATE,

    OP_INVALID,
    OP_HEAD,
  } req_op_e;

  struct Request
  {
    int64_t req_size; // 请求大小
    uint64_t id;      // 请求的对象id
    int64_t req_num;  // 请求计数，逻辑时间戳
    uint64_t time;    // 请求时间戳
    req_op_e type;    // 请求类型

    int64_t next_access_vtime; // 下一次访问的逻辑时间戳
    req_op_e next_access_op;   // 下一次访问的请求类型
    int64_t future_invalid_time;
    int64_t oracle_count;

    int32_t ttl;
    bool valid;

    //----负载分析中使用----

    bool overwrite;            // 是否覆写了之前的一个对象
    int32_t create_rtime;      // 创建当前对象时的实际时间戳
    int32_t create_vtime;      // 创建当前对象时的逻辑时间戳
    bool first_seen_in_window; // 当前对象在当前时间窗口中是否首次出现
    bool compulsory_miss;      // 当前对象是否是第一次出现，即冷不命中

    int64_t vtime_since_last_access; // 自上次访问以来经过的虚拟逻辑时间
    int64_t rtime_since_last_access; // 自上次访问以来经过的实际时间

    int64_t vtime_since_last_update; // 自上次更新以来经过的虚拟逻辑时间
    int64_t rtime_since_last_update; // 自上次更新以来经过的实际时间

    int64_t prev_size; // 当前对象上一次请求时的大小

    // inline int64_t size() const { return req_size; }
  } __attribute__((packed));

  // 为Request分配空间，并初始化一些必要属性
  static inline Request *new_request(void)
  {
    Request *req = my_malloc(Request);
    memset(req, 0, sizeof(Request));
    req->req_size = 1;
    req->type = OP_INVALID;
    req->valid = true;
    req->id = 0;
    req->time = 0;
    // req->hv = 0;
    req->next_access_vtime = -2;
    req->ttl = 0;
    return req;
  }

  static inline void free_request(parser::Request *req) { my_free(parser::Request, req); }

  // VisitorFn 是一个函数指针
  // 指向的函数接收一个 const Request& 类型的参数
  // 这种类型的函数指针通常用于实现访问者模式，这是一种行为设计模式，允许你在不修改类的情况下增加新的操作
  typedef void (*VisitorFn)(const Request *);

  class Parser
  {
  public:
    Parser() {}
    virtual ~Parser() {}
    virtual void go(VisitorFn) = 0;

    virtual int read_one_req(Request *req) = 0;

    // 工厂模式，根据给定的配置，找到对应的负载格式，然后根据负载格式返回对应的 Parser 实例
    static Parser *create(const libconfig::Setting &settings);

    std::string trace_path_;
  };

}
