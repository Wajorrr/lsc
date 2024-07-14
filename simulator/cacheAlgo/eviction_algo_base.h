#pragma once

#include "cache_obj.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        cache_obj_t *q_head;
        cache_obj_t *q_tail;
    } FIFO_params_t;

    /* used by LFU related */
    typedef struct
    {
        cache_obj_t *q_head;
        cache_obj_t *q_tail;
    } LRU_params_t;

    /* used by LFU related */
    typedef struct freq_node
    {
        int64_t freq;
        cache_obj_t *first_obj;
        cache_obj_t *last_obj;
        uint32_t n_obj;
    } freq_node_t;

    typedef struct
    {
        cache_obj_t *q_head;
        cache_obj_t *q_tail;
        // clock uses one-bit counter
        int n_bit_counter;
        // max_freq = 1 << (n_bit_counter - 1)
        int max_freq;

        int64_t n_obj_rewritten;
        int64_t n_byte_rewritten;
    } Clock_params_t;

#ifdef __cplusplus
}
#endif
