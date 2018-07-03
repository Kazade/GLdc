#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>
#include "../containers/aligned_vector.h"

/* USAGE:
 *
 * profiler_init();
 *
 * void my_func() {
 *   TRACE_START();
 *
 *   ...
 *
 *   TRACE_CHECKPOINT();
 *
 *   ...
 *
 *   TRACE_CHECKPOINT();
 * }
 *
 * profiler_dump(1000);
 *
 *
 * Output:
 *
 * Name                Total Time          Breakdown
 * -------------------------------------------------
   my_func             15.33
                                           0: 4.23
                                           1: 11.10

 */

typedef struct {
    uint32_t total_time;
    uint32_t calls;
} Checkpoint;

typedef struct {
    char name[128];
    uint32_t calls;
    uint32_t start;
    uint32_t total_time;
    Checkpoint checkpoints[16];
    uint8_t max_checkpoints;
} Trace;

struct Profiler {
    Trace traces[256];
    uint8_t trace_count;
};

extern Profiler __profiler;

void profiler_init();
void profiler_dump(uint32_t ms);
void profiler_register_trace(Trace* trace, const char* func_name);
void profiler_trace_start(Trace* trace);
void profiler_trace_checkpoint(Trace* trace, int counter);

#define TRACE_START() \
    int __sc = 0; \
    static char __trace_registered = 0;
    static Trace __trace; \
    if(!_trace_registered) {profiler_register_trace(&__trace, __func__); __trace_registered = 1;} \
    profiler_trace_start(&__trace);

#define TRACE_CHECKPOINT() \
    profiler_trace_checkpoint(&__trace, __sc++);


#endif // PROFILE_H
