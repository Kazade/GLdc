#pragma once

#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int size;
    unsigned int capacity;
    unsigned char* data;
    unsigned int element_size;
} AlignedVector;

#define ALIGNED_VECTOR_CHUNK_SIZE 256u

void aligned_vector_init(AlignedVector* vector, unsigned int element_size);
void* aligned_vector_reserve(AlignedVector* vector, unsigned int element_count);
void* aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count);
void* aligned_vector_resize(AlignedVector* vector, const unsigned int element_count);
static inline void* aligned_vector_at(const AlignedVector* vector, const unsigned int index) {
    assert(index < vector->size);
    return &vector->data[index * vector->element_size];
}
void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count);
static inline void aligned_vector_clear(AlignedVector* vector){
    vector->size = 0;
}
void aligned_vector_shrink_to_fit(AlignedVector* vector);
void aligned_vector_cleanup(AlignedVector* vector);
static inline void* aligned_vector_back(AlignedVector* vector){
    return aligned_vector_at(vector, vector->size - 1);
}

#ifdef __cplusplus
}
#endif
