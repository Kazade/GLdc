#pragma once

#include <stdint.h>

typedef struct {
    char name[64];
    uint64_t start_time_in_us;
} Profiler;

#define PROFILING_COMPILED 0

#if PROFILING_COMPILED
Profiler* profiler_push(const char* name);
void _profiler_checkpoint(const char* name);
void _profiler_pop();

void _profiler_print_stats();

void _profiler_enable();
void _profiler_disable();

#else
#define profiler_push(name);
#define profiler_checkpoint(name);
#define profiler_pop();

#define profiler_print_stats();

#define profiler_enable();
#define profiler_disable();

#endif
