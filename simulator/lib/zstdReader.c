
#include "zstdReader.h"

#include <assert.h>
#include <errno.h> // errno
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strerror
#include <sys/stat.h>
#include <zstd.h>

#include "../common/logging.h"

#define LINE_DELIM '\n'

zstd_reader *create_zstd_reader(const char *trace_path)
{
    zstd_reader *reader = malloc(sizeof(zstd_reader));

    reader->ifile = fopen(trace_path, "rb");
    if (reader->ifile == NULL)
    {
        printf("cannot open %s\n", trace_path);
        exit(1);
    }

    reader->buff_in_sz = ZSTD_DStreamInSize();    // 输入缓冲区的大小
    reader->buff_in = malloc(reader->buff_in_sz); // 输入缓冲区

    reader->input.src = reader->buff_in; // 将输入缓冲区的地址赋值给ZSTD_inBuffer结构体的src字段
    reader->input.size = 0;              // ZSTD_inBuffer的缓冲区大小
    reader->input.pos = 0;               // ZSTD_inBuffer的当前写入位置(从ZSTD_outBuffer读取解压的数据写入到该位置)

    reader->buff_out_sz = ZSTD_DStreamOutSize() * 2; // 输出缓冲区的大小
    reader->buff_out = malloc(reader->buff_out_sz);  // 输出缓冲区

    reader->output.dst = reader->buff_out;     // 将输出缓冲区的地址赋值给ZSTD_outBuffer结构体的dst字段
    reader->output.size = reader->buff_out_sz; // ZSTD_outBuffer的缓冲区大小
    reader->output.pos = 0;                    // ZSTD_outBuffer的当前读取位置(从该位置读取数据进行解压)

    reader->buff_out_read_pos = 0; // 指示我们自己当前实际已经从输出缓冲区读取了多少数据
    reader->status = 0;            // 读取器的状态(错误、正常、结束)

    reader->zds = ZSTD_createDStream(); // 创建一个Zstd解压缩流，用于解压缩数据

    return reader;
}

void free_zstd_reader(zstd_reader *reader)
{
    ZSTD_freeDStream(reader->zds);
    free(reader->buff_in);
    free(reader->buff_out);
    free(reader);
}

size_t _read_from_file(zstd_reader *reader)
{
    size_t read_sz;

    // reader->buff_in，指向要读取数据的缓冲区
    // 1表示1字节，每个数据元素的大小
    // reader->buff_in_sz，表示要读取的数据元素的数量，我们想要读取的字节数等于缓冲区的大小
    // reader->ifile，指向要读取数据的文件
    read_sz = fread(reader->buff_in, 1, reader->buff_in_sz, reader->ifile);

    // 读取的字节数小于缓冲区的大小，说明已经读取到文件末尾
    if (read_sz < reader->buff_in_sz)
    {
        if (feof(reader->ifile))
        {
            reader->status = MY_EOF;
        }
        else
        {
            assert(ferror(reader->ifile));
            reader->status = ERR;
            return 0;
        }
    }
    //  DEBUG("read %zu bytes from file\n", read_sz);

    // ZSTD_inBuffer输入缓冲区的大小和位置，用于解压缩
    reader->input.size = read_sz;
    reader->input.pos = 0;

    return read_sz;
}

rstatus _decompress_from_buff(zstd_reader *reader)
{
    /* move the unread decompressed data to the head of buff_out */
    // 未读的解压数据的起始位置
    void *buff_start = reader->buff_out + reader->buff_out_read_pos;
    // 未读的已解压数据的大小
    size_t buff_left_sz = reader->output.pos - reader->buff_out_read_pos;
    // 将未读的已解压数据从buff_start移动到buff_out缓冲区头部
    memmove(reader->buff_out, buff_start, buff_left_sz);

    // 由于ZSTD_outBuffer的src指向的就是buff_out缓冲区
    // 所以更新ZSTD_outBuffer的pos字段，和当前buff_out的有效数据末尾位置同步
    reader->output.pos = buff_left_sz;
    // 更新buff_out_read_pos
    reader->buff_out_read_pos = 0;
    size_t old_pos = buff_left_sz;

    // 若输入缓冲区中的数据已经全部解压读取完毕，则从压缩文件中读取新的数据
    if (reader->input.pos >= reader->input.size)
    {
        size_t read_sz = _read_from_file(reader);
        if (read_sz == 0)
        {
            if (reader->status == MY_EOF)
            {
                return MY_EOF;
            }
            else
            {
                ERROR("read from file error\n");
                return ERR;
            }
        }
    }

    // 使用ZSTD_decompressStream函数来解压数据流
    // reader->zds，这是一个ZSTD_DStream类型的指针，表示一个Zstd解压缩流
    // reader->output，这是一个ZSTD_outBuffer类型的结构体，表示输出缓冲区，用于存储解压缩后的数据
    // reader->input，这是一个ZSTD_inBuffer类型的结构体，表示输入缓冲区，其中包含要解压缩的数据
    // 返回一个size_t类型的值，表示还需要解压缩的数据的大小
    // 如果ret等于0，那么表示所有的数据都已经解压缩完毕
    // 如果ret小于0，那么表示解压缩过程中发生了错误
    size_t const ret =
        ZSTD_decompressStream(reader->zds, &(reader->output), &(reader->input));
    if (ret != 0)
    {
        if (ZSTD_isError(ret))
        {
            printf("%zu\n", ret);
            WARN("zstd decompression error: %s\n", ZSTD_getErrorName(ret));
        }
    }
    //  DEBUG("decompress %zu - %zu bytes\n", reader->output.pos, old_pos);

    return OK;
}

/**
    *line_start points to the start of the new line
    *line_end   points to the \n

    @return the number of bytes read (include line ending byte)
**/
size_t zstd_reader_read_line(zstd_reader *reader, char **line_start,
                             char **line_end)
{
    bool has_data_in_line_buff = false;

    if (reader->buff_out_read_pos < reader->output.pos)
    {
        /* there are still content in output buffer */
        *line_start = reader->buff_out + reader->buff_out_read_pos;
        void *buff_start = reader->buff_out + reader->buff_out_read_pos;
        size_t buff_left_sz = reader->output.pos - reader->buff_out_read_pos;
        *line_end = memchr(buff_start, LINE_DELIM, buff_left_sz);
        if (*line_end == NULL)
        {
            /* cannot find end of line, copy left over bytes, and decompress the next
             * frame */
            has_data_in_line_buff = true;
        }
        else
        {
            /* find a line in buff_out */
            assert(**line_end == LINE_DELIM);
            size_t sz = *line_end - *line_start + 1;
            reader->buff_out_read_pos += sz;

            return sz;
        }
    }

    rstatus status = _decompress_from_buff(reader);
    if (status != OK)
    {
        if (status == MY_EOF && has_data_in_line_buff)
        {
            *(((char *)(reader->buff_out)) + reader->output.pos) = '\n';
        }
        else
        {
            return 0;
        }
    }
    else if (reader->output.pos < reader->output.size / 4)
    {
        /* input buffer does not have enough content, read more from file */
        status = _decompress_from_buff(reader);
    }

    *line_start = reader->buff_out + reader->buff_out_read_pos;
    *line_end = memchr(*line_start, LINE_DELIM, reader->output.pos);
    // printf("start at %d %d end %d %d\n", reader->buff_out_read_pos,
    // **line_start, *line_end - *line_start, **line_end);
    assert(*line_end != NULL);
    assert(**line_end == LINE_DELIM);
    size_t sz = *line_end - *line_start + 1;
    reader->buff_out_read_pos = sz;

    return sz;
}

/**
 * read n_byte from reader, decompress if needed, data_start points to the new
 * data
 *
 * return the number of available bytes
 *
 * @param reader
 * @param n_byte
 * @param data_start
 * @return
 */
size_t zstd_reader_read_bytes(zstd_reader *reader, size_t n_byte,
                              char **data_start)
{
    size_t sz = 0;
    // 若当前要读取的字节数超出了输出缓冲区中可读取的有效数据大小，则需要解压缩新的数据
    while (reader->buff_out_read_pos + n_byte > reader->output.pos)
    {
        // 从输入缓冲区中解压缩新的数据并写入到输出缓存区
        rstatus status = _decompress_from_buff(reader);

        if (status != OK)
        {
            if (status != MY_EOF)
            {
                ERROR("error decompress file\n");
            }
            else
            {
                /* end of file */
                return 0;
            }
            break;
        }
    }

    // 若输出缓冲区中的有效数据足够多，直接从输出缓冲区中读取n_byte字节的数据
    if (reader->buff_out_read_pos + n_byte <= reader->output.pos)
    {
        sz = n_byte;
        // 将data_start指向输出缓冲区中的有效数据起始位置
        *data_start = ((char *)reader->buff_out) + reader->buff_out_read_pos;
        // 更新buff_out_read_pos，指向还未读取的数据的起始位置
        reader->buff_out_read_pos += n_byte;

        return sz; // 返回读取的字节数
    }
    else // 解压缩之后，剩余数据仍然不够n_byte字节
    {
        ERROR("do not have enough bytes %zu < %zu\n",
              reader->output.pos - reader->buff_out_read_pos, n_byte);

        return sz;
    }
}