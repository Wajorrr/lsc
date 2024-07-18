#pragma once

#include <libconfig.h++>

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
    int64_t next_access_vtime;
    req_op_e next_access_op;
    int64_t future_invalid_time;
    int64_t oracle_count;

    int32_t ttl;
    bool valid;

    // inline int64_t size() const { return req_size; }
  } __attribute__((packed));

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

    // 工厂模式，根据给定的配置，找到对应的负载格式，然后根据负载格式返回对应的 Parser 实例
    static Parser *create(const libconfig::Setting &settings);
  };

}
