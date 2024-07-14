#pragma once

#include <stdio.h>
#include <zstd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        ERR,
        OK,
        MY_EOF
    } rstatus;

    /**
     * Zstandard（也称为zstd）是一种实时压缩算法，提供了高压缩比和快速压缩速度。
     * 它由 Facebook 开发并开源，现在已经被广泛应用于各种场景，包括日志压缩、数据存储和网络传输等。
     */
    typedef struct zstd_reader
    {
        FILE *ifile;       // 文件指针
        ZSTD_DStream *zds; // 指向 Zstandard 解压流的指针。Zstandard 库使用这种类型的对象来跟踪解压过程的状态。

        size_t buff_in_sz;  // 输入缓冲区的大小
        void *buff_in;      // 输入缓冲区
        size_t buff_out_sz; // 输出缓冲区的大小
        void *buff_out;     // 输出缓冲区

        size_t buff_out_read_pos; // 已经从输出缓冲区读取了多少数据

        // Zstandard 库用于管理输入和输出数据的结构体
        // 包含了缓冲区的指针、大小以及已经处理的数据量
        ZSTD_inBuffer input;
        ZSTD_outBuffer output;

        rstatus status; // 当前读取器的状态(错误、正常、结束)
    } zstd_reader;

    // 创建一个 zstd_reader 对象
    zstd_reader *create_zstd_reader(const char *trace_path);

    // 释放对象
    void free_zstd_reader(zstd_reader *reader);

    // 从 zstd_reader 对象中读取一行解压后的数据
    size_t zstd_reader_read_line(zstd_reader *reader, char **line_start,
                                 char **line_end);

    // 从 zstd_reader 对象中读取 n_byte 字节的数据
    /* read n_byte from reader, decompress if needed, data_start points to the new
     * data */
    size_t zstd_reader_read_bytes(zstd_reader *reader, size_t n_byte,
                                  char **data_start);

#ifdef __cplusplus
}
#endif
