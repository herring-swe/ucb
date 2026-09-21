/* A pure C callable used to verify the C++ microbench harness. */

#include <stdint.h>

void microbench_c_function(void)
{
    static volatile uint32_t value = 1;
    value = value * 3U + 1U;
}

/* Opaque escape barrier: passing an array to this function prevents the
 * optimizer from eliminating benchmark loops whose results are otherwise
 * unused. */
void ucb_bench_sink_ptr(const void* ptr)
{
    (void)ptr;
}