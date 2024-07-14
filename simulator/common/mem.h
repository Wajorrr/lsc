#ifndef MEM_H
#define MEM_H

#include "config.h"

// 根据不同的堆分配器配置提供内存分配和释放的宏定义，这些宏定义允许在不同的内存管理策略之间进行切换

// Glib的堆分配器，glib.h头文件
// 使用Glib的g_new和g_free函数进行内存分配和释放。这种方式适用于使用Glib库的项目
#if HEAP_ALLOCTOR == HEAP_ALLOCATOR_G_NEW
#include "glib.h"
#define my_malloc(type) g_new(type, 1)
#define my_malloc_n(type, n) g_new(type, n)
#define my_free(size, addr) g_free(addr)

// Glib的切片分配器，gmodule.h头文件
// Glib提供的另一种内存管理机制，它可以提供更高效的内存使用和更快的分配速度，特别是对于小块内存的分配
#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_G_SLICE_NEW
#include "gmodule.h"
#define my_malloc(type) g_slice_new(type)
#define my_malloc_n(type, n) (type *)g_slice_alloc(sizeof(type) * n)
#define my_free(size, addr) g_slice_free1(size, addr)

// 标准的malloc和calloc，stdlib.h头文件
// 使用标准C库的malloc和calloc函数
#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_MALLOC
#include <stdlib.h>
#define my_malloc(type) (type *)malloc(sizeof(type))
#define my_malloc_n(type, n) (type *)calloc(sizeof(type), n)
#define my_free(size, addr) free(addr)

// 对齐的内存分配，stdlib.h头文件
// aligned_alloc允许分配具有特定对齐要求的内存块，这在某些硬件操作或特定性能优化场景中非常有用
#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_ALIGNED_MALLOC
#include <stdlib.h>
#define my_malloc(type) (type *)aligned_alloc(MEM_ALIGN_SIZE, sizeof(type));
#define my_malloc_n(type, n) \
    (type *)aligned_alloc(MEM_ALIGN_SIZE, sizeof(type) * n)
#define my_free(size, addr) free(addr)
#endif

#endif
