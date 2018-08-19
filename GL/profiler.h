#pragma once

#include <stdint.h>

typedef struct {
    char name[64];
    uint64_t start_time_in_us;
} Profiler;


Profiler* profiler_push(const char* name);
void profiler_checkpoint(const char* name);
void profiler_pop();

void profiler_print_stats();

void profiler_enable();
void profiler_disable();
