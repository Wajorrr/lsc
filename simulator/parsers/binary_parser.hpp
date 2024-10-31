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
#include <fcntl.h>
#include <sys/mman.h>

#include "../lib/zstdReader.h"
#include "../lib/binaryUtils.h"
#include "parser.hpp"
#include "../common/logging.h"

typedef double double64_t;

namespace parser
{

    class BinaryParser : public virtual Parser
    {

    public:
        BinaryParser(std::string trace_path, uint64_t _numRequests, std::string fmt_str, int32_t fields_num, int trace_start_offset = 0)
            : numRequests(_numRequests), fmt_str(fmt_str), fields_num(fields_num), trace_start_offset(trace_start_offset)
        {
            trace_path_ = trace_path;
            INFO("Parsing binary trace file: %s\n", trace_path.c_str());
            size_t slen = trace_path.length();
            if (trace_path.substr(slen - 4) == ".zst")
            {
                is_zstd_file = true;
                zstd_reader_p = create_zstd_reader(trace_path.c_str());
                INFO("opening a zstd compressed data\n");
            }

            int fd;
            struct stat st;
            // 打开文件，获取文件描述符
            if ((fd = open(trace_path.c_str(), O_RDONLY)) < 0)
            {
                ERROR("Unable to open '%s', %s\n", trace_path, strerror(errno));
                exit(1);
            }

            // 获取文件状态
            if ((fstat(fd, &st)) < 0)
            {
                close(fd);
                ERROR("Unable to fstat '%s', %s\n", trace_path, strerror(errno));
                exit(1);
            }
            // 文件大小
            file_size = st.st_size;

            mapped_file = static_cast<char *>(mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0));

#ifdef MADV_HUGEPAGE
            INFO("use hugepage\n");
            // 使用 madvise 函数为映射区域设置大页和顺序访问建议
            madvise(mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
#endif
            if ((mapped_file) == MAP_FAILED)
            {
                close(fd);
                mapped_file = NULL;
                ERROR("Unable to allocate %llu bytes of memory, %s\n",
                      (unsigned long long)st.st_size, strerror(errno));
                abort();
            }

            item_size = cal_offset(fmt_str.c_str(), fields_num + 1);
            if (item_size == 0)
            {
                ERROR("binaryReader_setup: item_size is 0, fmt \"%s\", %d fields\n",
                      fmt_str.c_str(), fields_num);
            }

            ssize_t data_region_size = file_size - trace_start_offset;
            if (data_region_size % item_size != 0)
            {
                WARN(
                    "trace file size %lu - %lu is not multiple of item size %lu, mod %lu\n",
                    (unsigned long)file_size,
                    (unsigned long)trace_start_offset,
                    (unsigned long)item_size,
                    (unsigned long)file_size % item_size);
            }
            totalRequests = (uint64_t)data_region_size / (item_size); // 对于bin文件可以计算请求总数，对于zstd压缩的文件无效
        }

        void go(VisitorFn visit) // VisitorFn visit为缓存的访问函数
        {
            uint32_t clock_time;
            uint64_t obj_id;
            uint64_t obj_size;
            parser::req_op_e op;
            uint64_t next_access_vtime;
            parser::req_op_e next_access_op;

            Request req;
            req.req_num = 0;
            while (true)
            {
                if (fmt_str == "IQQB")
                {
                    char *record = read_bytes();
                    if (record == NULL)
                    {
                        INFO("Read EOF, Processed %ld Requests\n", req.req_num);
                        break;
                    }
                    clock_time = *(uint32_t *)record;
                    obj_id = *(uint64_t *)(record + 4);
                    obj_size = *(uint64_t *)(record + 12);
                    op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 20));
                }
                else if (fmt_str == "IQQBQB")
                {
                    char *record = read_bytes();
                    if (record == NULL)
                    {
                        INFO("Read EOF, Processed %ld Requests\n", req.req_num);
                        break;
                    }
                    clock_time = *(uint32_t *)record;
                    obj_id = *(uint64_t *)(record + 4);
                    obj_size = *(uint64_t *)(record + 12);
                    op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 20));
                    next_access_vtime = *(uint64_t *)(record + 21);
                    next_access_op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 29));
                }
                else
                {
                    ERROR("unknown format string %s\n", fmt_str.c_str());
                }

                req.id = obj_id;
                req.req_size = obj_size;
                req.time = clock_time;
                req.type = op;
                req.next_access_vtime = next_access_vtime;
                req.next_access_op = next_access_op;
                req.req_num++;
                if (req.req_num <= 10)
                    INFO("id:%" PRIu64 ",req_size:%ld,time:%ld,type:%d,next_access_vtime:%ld,next_access_op:%d\n",
                         req.id, req.req_size, req.time, req.type, req.next_access_vtime, req.next_access_op);
                if (req.req_size >= MAX_TAO_SIZE)
                {
                    req.req_size = MAX_TAO_SIZE - 1;
                }
                visit(&req);
                if (numRequests < 0) // numRequests < 0表示不限制请求数量
                    continue;
                if (numRequests != 0)
                    numRequests--;
                else
                {
                    INFO("Finished Processing %ld Requests\n", req.req_num);
                    break;
                }
            }
        }

        int read_one_req(parser::Request *req)
        {
            uint32_t clock_time;
            uint64_t obj_id;
            uint64_t obj_size;
            parser::req_op_e op;
            uint64_t next_access_vtime;
            parser::req_op_e next_access_op;

            if (fmt_str == "IQQB")
            {
                char *record = read_bytes();
                if (record == NULL)
                {
                    INFO("Read EOF, Processed %ld Requests\n", req->req_num);
                    return 0;
                }
                clock_time = *(uint32_t *)record;
                obj_id = *(uint64_t *)(record + 4);
                obj_size = *(uint64_t *)(record + 12);
                op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 20));
            }
            else if (fmt_str == "IQQBQB")
            {
                char *record = read_bytes();
                if (record == NULL)
                {
                    INFO("Read EOF, Processed %ld Requests\n", req->req_num);
                    return 0;
                }
                clock_time = *(uint32_t *)record;
                obj_id = *(uint64_t *)(record + 4);
                obj_size = *(uint64_t *)(record + 12);
                op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 20));
                next_access_vtime = *(uint64_t *)(record + 21);
                next_access_op = static_cast<parser::req_op_e>(*(uint8_t *)(record + 29));
            }
            else
            {
                ERROR("unknown format string %s\n", fmt_str.c_str());
            }

            req->id = obj_id;
            req->req_size = obj_size;
            req->time = clock_time;
            req->type = op;
            req->next_access_vtime = next_access_vtime;
            req->next_access_op = next_access_op;
            req->req_num++;
            if (req->req_num <= 10)
                INFO("id:%" PRIu64 ",req_size:%ld,time:%ld,type:%d,next_access_vtime:%ld,next_access_op:%d\n",
                     req->id, req->req_size, req->time, req->type, req->next_access_vtime, req->next_access_op);
            // if (req->req_size >= MAX_TAO_SIZE)
            // {
            //     req->req_size = MAX_TAO_SIZE - 1;
            // }
            if (numRequests < 0) // numRequests < 0表示不限制请求数量
                return 1;
            if (numRequests != 0)
            {
                numRequests--;
                return 1;
            }
            else
            {
                INFO("Finished Processing %ld Requests\n", req->req_num);
                return 0;
            }
        }

        // 用于直接读取未解压的数据文件
        inline char *read_bytes()
        {

            char *start = NULL;

            if (!is_zstd_file)
            {
                // 偏移量超过了文件末尾
                if (mmap_offset >= file_size)
                    return NULL;

                // 当前请求的起始位置
                start = (mapped_file + mmap_offset);
                // 更新偏移量，指向下一个请求的起始位置
                mmap_offset += item_size;
            }
            else
            {
                // 从zstd压缩文件中读取item_size字节的数据，返回值为读取的字节数
                size_t sz =
                    zstd_reader_read_bytes(zstd_reader_p, item_size, &start);
                if (sz == 0)
                {
                    if (zstd_reader_p->status != MY_EOF)
                    {
                        ERROR("fail to read zstd trace\n");
                    }
                    return NULL;
                }
            }

            // 返回当前请求的起始位置
            return start;
        }

    private:
        uint64_t numRequests;
        int64_t totalRequests;

        std::string fmt_str;    // 一行请求各个字段的格式
        int32_t fields_num;     // 一行请求的字段个数
        size_t item_size;       // 一行请求的各个字段的总大小
        size_t file_size;       // 文件大小
        int trace_start_offset; // 读取的起始位置

        char *mapped_file;  // 映射文件的指针
        size_t mmap_offset; // 映射文件的偏移量

        struct zstd_reader *zstd_reader_p; // 用于读取zstd文件
        bool is_zstd_file;                 // 是否是 zstd 格式
    };

} // namespace parser
