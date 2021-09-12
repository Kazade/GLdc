#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__) || defined(__WIN32__)
/* Linux + Kos define this, OSX does not, so just use malloc there */
static inline void* memalign(size_t alignment, size_t size) {
    return malloc(size);
}
#else
    #include <malloc.h>
#endif

typedef struct {
    unsigned int size;
    unsigned int capacity;
    unsigned char* data;
    unsigned int element_size;
} AlignedVector;

#define ALIGNED_VECTOR_CHUNK_SIZE 256u

#ifdef __cplusplus
#define AV_FORCE_INLINE static inline
#else
#define AV_NO_INSTRUMENT inline __attribute__((no_instrument_function))
#define AV_INLINE_DEBUG AV_NO_INSTRUMENT __attribute__((always_inline))
#define AV_FORCE_INLINE static AV_INLINE_DEBUG
#endif

#define ROUND_TO_CHUNK_SIZE(v) \
    ((((v) + ALIGNED_VECTOR_CHUNK_SIZE - 1) / ALIGNED_VECTOR_CHUNK_SIZE) * ALIGNED_VECTOR_CHUNK_SIZE)


void aligned_vector_init(AlignedVector* vector, unsigned int element_size);

AV_FORCE_INLINE void* aligned_vector_reserve(AlignedVector* vector, unsigned int element_count) {
    if(element_count == 0) {
        return NULL;
    }

    if(element_count <= vector->capacity) {
        return NULL;
    }

    unsigned int original_byte_size = vector->size * vector->element_size;

    /* We overallocate so that we don't make small allocations during push backs */
    element_count = ROUND_TO_CHUNK_SIZE(element_count);

    unsigned int new_byte_size = element_count * vector->element_size;
    unsigned char* original_data = vector->data;

    vector->data = (unsigned char*) memalign(0x20, new_byte_size);
    assert(vector->data);

    if(original_data) {
        memcpy(vector->data, original_data, original_byte_size);
        free(original_data);
    }

    vector->capacity = element_count;

    return vector->data + original_byte_size;
}

AV_FORCE_INLINE void* aligned_vector_at(const AlignedVector* vector, const unsigned int index) {
    assert(index < vector->size);
    return &vector->data[index * vector->element_size];
}

AV_FORCE_INLINE void* aligned_vector_resize(AlignedVector* vector, const unsigned int element_count) {
    void* ret = NULL;

    unsigned int previousCount = vector->size;

    /* Don't change memory when resizing downwards, just change the size */
    if(element_count <= vector->size) {
        vector->size = element_count;
        return NULL;
    }

    if(vector->capacity < element_count) {
        ret = aligned_vector_reserve(vector, element_count);
        vector->size = element_count;
    } else if(previousCount < element_count) {
        vector->size = element_count;
        ret = aligned_vector_at(vector, previousCount);
    }

    if(previousCount < vector->size) {
        return ret;
    } else {
        return NULL;
    }
}

AV_FORCE_INLINE void* aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count) {
    /* Resize enough room */
    assert(count);
    assert(vector->element_size);

    unsigned int initial_size = vector->size;
    aligned_vector_resize(vector, vector->size + count);

    assert(vector->size == initial_size + count);

    unsigned char* dest = vector->data + (vector->element_size * initial_size);

    /* Copy the objects in */
    memcpy(dest, objs, vector->element_size * count);

    return dest;
}


AV_FORCE_INLINE void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count) {
    return aligned_vector_resize(vector, vector->size + additional_count);
}

AV_FORCE_INLINE void aligned_vector_clear(AlignedVector* vector){
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
