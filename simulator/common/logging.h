#ifndef LOGGING_H
#define LOGGING_H

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "const.h"

#ifdef __cplusplus
extern "C"
{
#endif

    static inline void log_header(int level, const char *file, int line);
    void print_stack_trace(void);
    void print_progress(double perc);
    extern pthread_mutex_t log_mtx;

//  1、锁定一个全局互斥锁log_mtx，以确保在多线程环境中写日志的操作是线程安全的
//  2、输出日志头部，通常包含日志级别、时间戳、文件名和行号等信息
//  3、输出实际的日志消息
//  4、重置控制台颜色，其中NORMAL是一个预定义的宏，代表控制台的默认颜色。这是为了避免日志消息中的颜色设置影响到后续的控制台输出
//  5、确保立即将缓冲的日志信息输出到标准输出，而不是等到缓冲区满了或者程序正常结束时才输出
//  6、解锁之前锁定的互斥锁，允许其他线程进行日志记录。
#define LOGGING(level, FMT, ...)               \
    do                                         \
    {                                          \
        pthread_mutex_lock(&log_mtx);          \
        log_header(level, __FILE__, __LINE__); \
        printf(FMT, ##__VA_ARGS__);            \
        printf("%s", NORMAL);                  \
        fflush(stdout);                        \
        pthread_mutex_unlock(&log_mtx);        \
    } while (0)

#if LOGLEVEL <= VVVERBOSE_LEVEL // 非常非常详细（VVVERBOSE）
#define VVVERBOSE(FMT, ...) LOGGING(VVVERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
#define VVVERBOSE(FMT, ...)
#endif

#if LOGLEVEL <= VVERBOSE_LEVEL // 非常详细（VVERBOSE）
#define VVERBOSE(FMT, ...) LOGGING(VVERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
#define VVERBOSE(FMT, ...)
#endif

#if LOGLEVEL <= VERBOSE_LEVEL // 详细（VERBOSE）
#define VERBOSE(FMT, ...) LOGGING(VERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
#define VERBOSE(FMT, ...)
#endif

#if LOGLEVEL <= DEBUG_LEVEL // 调试（DEBUG）
#define DEBUG(FMT, ...) LOGGING(DEBUG_LEVEL, FMT, ##__VA_ARGS__)
#else
#define DEBUG(FMT, ...)
#endif

#if LOGLEVEL <= INFO_LEVEL // 信息（INFO）
#define INFO(FMT, ...) LOGGING(INFO_LEVEL, FMT, ##__VA_ARGS__)
#else
#define INFO(FMT, ...)
#endif

#if LOGLEVEL <= WARN_LEVEL // 警告（WARN）
#define WARN(FMT, ...) LOGGING(WARN_LEVEL, FMT, ##__VA_ARGS__)
#else
#define WARN(FMT, ...)
#endif

#if LOGLEVEL <= SEVERE_LEVEL // 严重错误（SEVERE）
#define ERROR(FMT, ...)                            \
    {                                              \
        LOGGING(SEVERE_LEVEL, FMT, ##__VA_ARGS__); \
        abort();                                   \
    }
#else
#define ERROR(FMT, ...)
#endif

// WARN_ONCE、DEBUG_ONCE和INFO_ONCE宏是特殊的日志宏，它们确保相应的日志消息只被记录一次。
// 它们通过在静态局部变量printed中存储状态来实现这一点
#define WARN_ONCE(FMT, ...)           \
    do                                \
    {                                 \
        static bool printed = false;  \
        if (!printed)                 \
        {                             \
            WARN(FMT, ##__VA_ARGS__); \
            printed = true;           \
            fflush(stdout);           \
        }                             \
    } while (0)

#define DEBUG_ONCE(FMT, ...)          \
    do                                \
    {                                 \
        static bool printed = false;  \
        if (!printed)                 \
        {                             \
            WARN(FMT, ##__VA_ARGS__); \
            printed = true;           \
            fflush(stdout);           \
        }                             \
    } while (0)

#define INFO_ONCE(FMT, ...)           \
    do                                \
    {                                 \
        static bool printed = false;  \
        if (!printed)                 \
        {                             \
            WARN(FMT, ##__VA_ARGS__); \
            printed = true;           \
            fflush(stdout);           \
        }                             \
    } while (0)

    static inline void log_header(int level, const char *file, int line)
    {
        //  if (level < LOGLEVEL) {
        //    return 0;
        //  }

        // printf("%d\n", level);
        // printf("%s\n", file);
        // printf("%d\n", line);
        // printf("%d\n", strcmp(CYAN, "\x1B[36m"));
        // printf("%s[VVV]\n", "\x1B[36m");
        // printf("%s[VVV]   \n", CYAN);

        switch (level)
        {
        case VVVERBOSE_LEVEL:
            printf("%s[VVV]   ", CYAN);
            break;
        case VVERBOSE_LEVEL:
            printf("%s[VV]    ", CYAN);
            break;
        case VERBOSE_LEVEL:
            printf("%s[VERB]  ", MAGENTA);
            break;
        case DEBUG_LEVEL:
            printf("%s[DEBUG] ", CYAN);
            break;
        case INFO_LEVEL:
            printf("%s[INFO]  ", GREEN);
            break;
        case WARN_LEVEL:
            printf("%s[WARN]  ", YELLOW);
            break;
        case SEVERE_LEVEL:
            printf("%s[ERROR] ", RED);
            break;
        default:
            printf("in logging should not be here\n");
            break;
        }

        char buffer[30];
        struct timeval tv;
        time_t curtime;

        // 获取当前的时间，并将其转换为"%m-%d-%Y %T"格式的字符串
        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;

        strftime(buffer, 30, "%m-%d-%Y %T", localtime(&curtime));

        // 从文件名字符串中找到最后一个/字符的位置，然后加1
        // 这样做的目的是只输出源文件的名称，而不是完整的路径，以便于日志的阅读
        // 若文件名中不包含/字符，则直接输出文件名
        printf("%s %8s:%-4d ", buffer, strrchr(file, '/') ? strrchr(file, '/') + 1 : file, line);

        // 输出线程ID
        printf("(tid=%zu): ", (unsigned long)pthread_self());
    }

#ifdef __cplusplus
}
#endif

#endif
