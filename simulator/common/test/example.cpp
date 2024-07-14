#include <stdio.h>
#include <execinfo.h>
#include <unistd.h>

#include "../logging.h"

#define LOGLEVEL VVVERBOSE_LEVEL

using namespace std;

// 一个示例函数，用于演示调用print_stack_trace
void my_function(void)
{
    print_stack_trace();
}

void show_progress()
{
    double x = 0;
    for (int i = 0; i < 50; i++)
    {
        x += 2;
        print_progress(x);
        sleep(0.1);
    }
}

int main(void)
{
    my_function(); // 在这里调用my_function，它会进一步调用print_stack_trace
    show_progress();
    VVVERBOSE("This is a very very verbose message %d\n", 1);
    VVERBOSE("This is a very verbose message %d\n", 2);
    VERBOSE("This is a verbose message %d\n", 3);
    DEBUG("This is a debug message %d\n", 4);
    INFO("This is an info message %d\n", 5);
    WARN("This is a warning message %d\n", 6);
    ERROR("This is a severe message %d\n", 7);
    return 0;
}