#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int alloc_init(void* pool, size_t size);
void alloc_shutdown(void* pool);

void *alloc_malloc(void* pool, size_t size);
void alloc_free(void* pool, void* p);

void alloc_defrag_start(void* pool);
void* alloc_defrag_address(void* pool, void* p);
void alloc_defrag_commit(void* pool);
bool alloc_defrag_in_progress(void* pool);

size_t alloc_count_free(void* pool);
size_t alloc_count_continuous(void* pool);

void* alloc_next_available(void* pool, size_t required_size);
void* alloc_base_address(void* pool);
size_t alloc_block_count(void* pool);

#ifdef __cplusplus
}
#endif
