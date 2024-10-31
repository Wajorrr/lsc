#pragma once

// disable threading as there seems to be a bug in csv.h
#define CSV_IO_NO_THREAD 1

#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <stdint.h>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_set>

#include "../constants.hpp"
#include "../lib/csv.h"
#include "parser.hpp"
#include "../common/logging.h"

namespace parser
{

    typedef float float32_t;

    class MetaKVParser : public virtual Parser
    {
    public:
        MetaKVParser(std::string filename, uint64_t _numRequests, double _sampling, double _seed, double _object_scaling)
        {
            std::cout << "Parsing MetaKV, object file: " << filename << std::endl;

            reader = new MetaKVCSVformat(filename);
            // key,op,size,op_count,key_size
            reader->set_header("key", "op", "size", "op_count", "key_size");
            reader->read_header(io::ignore_no_column, "key", "op", "size", "op_count", "key_size");

            numRequests = _numRequests;
            totalRequests = _numRequests;
            sampling = _sampling;
            scaling = _object_scaling;
            rest_count = 0;
            if (sampling != 1)
            {
                randomGenerator.seed(_seed);
                unif = std::uniform_real_distribution<double>(0.0, 1.0);
            }
        }

        void go(VisitorFn visit)
        {
            int64_t shard = 0;

            std::string key;
            std::string op;
            int64_t size = 0;
            size_t op_count = 0;
            int64_t key_size = 0;

            Request req;
            req.req_num = 0;
            while (reader->read_row(key, op, size, op_count, key_size))
            {
                // if (req.req_num <= 10)
                // INFO("key:%s,op:%s,size:%ld,op_count:%ld,key_size:%ld\n", key.c_str(), op.c_str(), size, op_count, key_size);

                if (numRequests == 0)
                {
                    std::cout << "Finished Processing "
                              << totalRequests << " Requests\n";
                    return;
                }

                if (sampling != 1 && (selected_items.find(shard) == selected_items.end()))
                {
                    if (discarded_items.find(shard) == discarded_items.end())
                    {
                        double currentRandom = unif(randomGenerator);
                        if (currentRandom < sampling) // 根据采样率和随机数判断是否采样
                        {
                            selected_items.insert(shard);
                        }
                        else
                        {
                            discarded_items.insert(shard);
                            continue;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }

                req.id = std::hash<std::string>{}(key);
                req.req_size = size + key_size;
                double s = req.req_size * scaling;
                req.req_size = round(s);

                // 这里用于控制kv的大小，若过大，放不进一个4kb block，则应特殊处理
                // 这里是直接设置成大小上限了
                if (req.req_size >= MAX_TAO_SIZE)
                {
                    req.req_size = MAX_TAO_SIZE - 1;
                }
                if (req.req_size == 0)
                {
                    req.req_size = 1;
                }

                // if (op == "SET")
                // {
                //     req.type = parser::SET;
                // }
                // else if (op == "GET")
                // {
                //     req.type = parser::GET;
                // }
                // else if (op == "DELETE")
                // {
                //     req.type = parser::DELETE;
                // }
                // else
                // {
                //     req.type = parser::OTHER;
                // }

                for (uint i = 0; i < op_count; i++)
                {
                    req.req_num++;
                    req.time = req.req_num;
                    visit(&req);
                    if (numRequests < 0) // numRequests < 0表示不限制请求数量
                        continue;
                    if (numRequests != 0)
                    {
                        numRequests--;
                    }
                    else
                    {
                        INFO("Finished Processing %ld Requests\n", req.req_num);
                        return;
                    }
                }
            }
            INFO("Read EOF, Processed %ld Requests\n", req.req_num);
        }

        int read_one_req(parser::Request *req)
        {
            if (rest_count)
            {
                rest_count--;
                return 1;
            }

            int64_t shard = 0;

            std::string key;
            std::string op;
            int64_t size = 0;
            size_t op_count = 0;
            int64_t key_size = 0;

            reader->read_row(key, op, size, op_count, key_size);

            req->id = std::hash<std::string>{}(key);
            req->req_size = size + key_size;
            double s = req->req_size * scaling;
            req->req_size = round(s);
            rest_count = op_count;

            // 这里用于控制kv的大小，若过大，放不进一个4kb block，则应特殊处理
            // 这里是直接设置成大小上限了
            if (req->req_size >= MAX_TAO_SIZE)
            {
                req->req_size = MAX_TAO_SIZE - 1;
            }
            if (req->req_size == 0)
            {
                req->req_size = 1;
            }

            // if (req->req_num <= 10)
            // if (req->req_num <= 1)
            //     INFO("id:%" PRIu64 ",req_size:%ld,time:%ld,type:%d,next_access_vtime:%ld,next_access_op:%d\n, future_invalid_time:%ld\n",
            //          req->id, req->req_size, req->time, req->type, req->next_access_vtime, req->next_access_op, req->future_invalid_time);

            // req->req_num++;

            if (numRequests < 0) // numRequests < 0表示不限制请求数量
                return 1;
            if (numRequests != 0)
                numRequests--;
            else
            {
                INFO("Finished Processing %ld Requests\n", req->req_num);
                return 0;
            }
        }

    private:
        typedef io::CSVReader<5, io::trim_chars<' '>, io::no_quote_escape<','>> MetaKVCSVformat;
        MetaKVCSVformat *reader;
        int64_t numRequests;
        int64_t totalRequests;
        double sampling;
        double scaling;
        std::mt19937 randomGenerator;
        std::uniform_real_distribution<double> unif;
        std::unordered_set<int64_t> selected_items;
        std::unordered_set<int64_t> discarded_items;
        int rest_count;
    };

} // namespace parser
