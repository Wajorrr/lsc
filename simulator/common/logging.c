#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;

// 打印当前线程的调用堆栈
void print_stack_trace(void)
{
    void *array[10];
    size_t size;

    // 调用backtrace函数获取当前线程的调用堆栈，将堆栈信息存储在array中
    // backtrace第二个参数指定了array的大小，也就是最多可以获取多少层的调用堆栈
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "stack trace: \n");
    // 调用 backtrace_symbols_fd 函数将堆栈信息解析为字符串并输出到标准错误输出
    backtrace_symbols_fd(array, size, STDERR_FILENO);
}

// 打印进度信息，传入的参数为当前进度百分比
void print_progress(double perc)
{
    // 用于存储上一次打印的进度和时间
    static double last_perc = 0;
    static time_t last_print_time = 0;
    time_t cur_time = time(NULL);

    // 检查当前的进度是否比上一次的进度增加了至少0.01%，并且当前时间是否比上一次打印的时间至少过去了60秒
    if (perc - last_perc < 0.01)
    // if (perc - last_perc < 0.01 || cur_time - last_print_time < 60)
    {
        last_perc = perc;
        last_print_time = cur_time;
        sleep(2);
        return;
    }

    // ANSI转义序列是一种在文本终端中控制光标位置、颜色等的方式。
    // \033是ASCII字符集中的ESC（Escape）字符的八进制表示
    // ESC字符用于引导一系列的字符，以实现在文本终端中的各种控制功能，如光标移动、颜色设置等
    // \033后面跟随的[字符，表示开始一个ANSI转义序列
    // ANSI转义序列以\033[（也可以写作\x1b[）开始，然后跟着一个或多个数字（用分号分隔），最后以一个字母结束。
    // \033[A的意思是光标上移一行，[A表示光标上移
    // \033[2K的意思是清除当前行的内容，2K表示清除整行
    // \r 会将光标移动到当前行的开始

    // 使用ANSI转义序列\033[A\033[2K\r将光标移动到上一行并清空该行
    if (last_perc != 0)
        fprintf(stdout, "\033[A\033[2K\r");
    // 打印新的进度信息
    fprintf(stdout, "%.2f%%...\n", perc);
    // 更新last_perc和last_print_time的值
    last_perc = perc;
    last_print_time = cur_time;
}
