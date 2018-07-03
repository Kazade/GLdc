#include "profile.h"

Profiler __profiler;

void profiler_init() {
    __profiler.trace_count = 0;
}

static uint32_t time_now_in_ms() {
    return timer_us_gettime64() / 1000;
}

static int compare(const void* trace1, const void* trace2) {
    Trace* t1 = (Trace*) trace1;
    Trace* t2 = (Trace*) trace2;
    return (t1->total_time < t2->total_time) ? -1 : (t2->total_time < t1->total_time) ? 1 : 0;
}

void profiler_dump(uint32_t ms) {
    static uint32_t time_since_last = time_now_in_ms();

    uint32_t now = time_now_in_ms();

    if((now - time_since_last) >= ms) {
        qsort(__profiler.traces, __profiler.trace_count, sizeof(Trace), compare);

        fprintf(stderr, "Function\t\t\t\tTotal Time\t\t\t\tBreakdown\n");
        for(uint8_t i = 0; i < __profiler.trace_count; ++i) {
            Trace* trace = __profiler.traces + i;
            fprintf(stderr, "%s\t\t\t\t%dms\n", trace->name, trace->total_time);

            for(uint8_t j = 0; j < trace->max_checkpoints; ++j) {
                fprintf(stderr, "\t\t\t\t\t\t\t\t%d: %dms\n", trace->checkpoints[j].total_time);
            }

            fprintf(stderr, "\n\n");
        }

        time_since_last -= ms;
    }
}

void profiler_register_trace(Trace* trace, const char* func_name) {
    __profiler.traces[__profiler.trace_count++] = trace;

    trace->calls = 0;
    trace->max_checkpoints = 0;
    trace->total_time = 0;
    trace->start = 0;

    strncpy(trace->name, func_name, 128);
}

void profiler_trace_start(Trace* trace) {
    trace->start = time_now_in_ms();
    trace->calls++;
}

void profiler_trace_checkpoint(Trace* trace, int counter) {
    uint32_t now = time_now_in_ms();
    uint32_t checkpoint_time = (now - trace->start);

    trace->total_time += checkpoint_time;
    trace->start = now;

    trace->checkpoints[counter].total_time + checkpoint_time;
    trace->checkpoints[counter].calls++;
}
