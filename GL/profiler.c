#include <kos.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "profiler.h"
#include "../containers/aligned_vector.h"

#if PROFILING_COMPILED

#define MAX_PATH 256

typedef struct {
    char name[MAX_PATH];

    uint64_t total_time_us;
    uint64_t total_calls;
} ProfilerResult;

typedef struct {
    AlignedVector stack;
    AlignedVector results;
    uint64_t start_time_in_us;
} RootProfiler;


static RootProfiler* root = NULL;

static char PROFILER_ENABLED = 0;

void profiler_enable() {
    PROFILER_ENABLED = 1;
}

void profiler_disable() {
    PROFILER_ENABLED = 0;
}

static ProfilerResult* profiler_get_or_create_result(const char* name) {
    if(!PROFILER_ENABLED) return NULL;

    uint16_t i = 0;
    for(; i < root->results.size; ++i) {
        ProfilerResult* result = aligned_vector_at(&root->results, i);
        if(strcmp(result->name, name) == 0) {
            return result;
        }
    }

    ProfilerResult newResult;
    strcpy(newResult.name, name);
    newResult.total_calls = 0;
    newResult.total_time_us = 0;
    aligned_vector_push_back(&root->results, &newResult, 1);
    return aligned_vector_back(&root->results);
}

static uint64_t current_time_in_us() {
    return timer_us_gettime64();
}

static void profiler_generate_path(const char* suffix, char* path) {
    uint16_t i = 0;
    for(; i < root->stack.size; ++i) {
        Profiler* prof = aligned_vector_at(&root->stack, i);
        strcat(path, prof->name);

        if(i != root->stack.size - 1) {
            strcat(path, ".");
        }
    }

    if(strlen(suffix)) {
        strcat(path, ":");
        strcat(path, suffix);
    }
}


Profiler* profiler_push(const char* name) {
    if(!PROFILER_ENABLED) return NULL;

    if(!root) {
        root = (RootProfiler*) malloc(sizeof(RootProfiler));
        aligned_vector_init(
            &root->stack,
            sizeof(Profiler)
        );

        aligned_vector_init(
            &root->results,
            sizeof(ProfilerResult)
        );

        aligned_vector_reserve(&root->stack, 32);
        aligned_vector_reserve(&root->results, 64);
    }

    Profiler profiler;
    strncpy(profiler.name, name, 64);
    profiler.start_time_in_us = current_time_in_us();

    aligned_vector_push_back(&root->stack, &profiler, 1);
    return aligned_vector_back(&root->stack);
}

void profiler_checkpoint(const char* name) {
    if(!PROFILER_ENABLED) return;

    Profiler* prof = aligned_vector_back(&root->stack);

    char path[MAX_PATH];
    path[0] = '\0';

    profiler_generate_path(name, path);

    uint64_t now = current_time_in_us();
    uint64_t diff = now - prof->start_time_in_us;
    prof->start_time_in_us = now;

    ProfilerResult* result = profiler_get_or_create_result(path);
    result->total_calls++;
    result->total_time_us += diff;
}

void profiler_pop() {
    if(!PROFILER_ENABLED) return;

    aligned_vector_resize(&root->stack, root->stack.size - 1);
}

void profiler_print_stats() {
    if(!PROFILER_ENABLED) return;

    fprintf(stderr, "%-60s%-20s%-20s%-20s\n", "Path", "Average", "Total", "Calls");

    uint16_t i = 0;
    for(; i < root->results.size; ++i) {
        ProfilerResult* result = aligned_vector_at(&root->results, i);
        float ms = ((float) result->total_time_us) / 1000.0f;
        float avg = ms / (float) result->total_calls;

        fprintf(stderr, "%-60s%-20f%-20f%" PRIu64 "\n", result->name, (double)avg, (double)ms, result->total_calls);
    }
}
#endif
