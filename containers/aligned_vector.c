#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#if defined(__APPLE__) || defined(__WIN32__)
/* Linux + Kos define this, OSX does not, so just use malloc there */
static inline void* memalign(size_t alignment, size_t size) {
    return malloc(size);
}
#else
    #include <malloc.h>
#endif

#ifdef _arch_dreamcast
#include "../GL/private.h"
#else
#define FASTCPY memcpy
#endif

#include "aligned_vector.h"

void aligned_vector_init(AlignedVector* vector, unsigned int element_size) {
    vector->size = vector->capacity = 0;
    vector->element_size = element_size;
    vector->data = NULL;

    /* Reserve some initial capacity */
    aligned_vector_reserve(vector, ALIGNED_VECTOR_CHUNK_SIZE);
}

static inline unsigned int round_to_chunk_size(unsigned int val) {
    const unsigned int n = val;
    const unsigned int m = ALIGNED_VECTOR_CHUNK_SIZE;

    return ((n + m - 1) / m) * m;
}

void* aligned_vector_reserve(AlignedVector* vector, unsigned int element_count) {
    if(element_count == 0) {
        return NULL;
    }

    if(element_count <= vector->capacity) {
        return NULL;
    }

    unsigned int original_byte_size = vector->size * vector->element_size;

    /* We overallocate so that we don't make small allocations during push backs */
    element_count = round_to_chunk_size(element_count);

    unsigned int new_byte_size = element_count * vector->element_size;
    unsigned char* original_data = vector->data;

    vector->data = (unsigned char*) memalign(0x20, new_byte_size);
    assert(vector->data);

    if(original_data) {
        FASTCPY(vector->data, original_data, original_byte_size);
        free(original_data);
    }

    vector->capacity = element_count;

    return vector->data + original_byte_size;
}

void* aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count) {
    /* Resize enough room */
    assert(count);
    assert(vector->element_size);

    unsigned int initial_size = vector->size;
    aligned_vector_resize(vector, vector->size + count);

    assert(vector->size == initial_size + count);

    unsigned char* dest = vector->data + (vector->element_size * initial_size);

    /* Copy the objects in */
    FASTCPY(dest, objs, vector->element_size * count);

    return dest;
}

void* aligned_vector_resize(AlignedVector* vector, const unsigned int element_count) {
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

void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count) {
    return aligned_vector_resize(vector, vector->size + additional_count);
}

void aligned_vector_shrink_to_fit(AlignedVector* vector) {
    if(vector->size == 0) {
        free(vector->data);
        vector->data = NULL;
        vector->capacity = 0;
    } else {
        unsigned int new_byte_size = vector->size * vector->element_size;
        unsigned char* original_data = vector->data;
        vector->data = (unsigned char*) memalign(0x20, new_byte_size);

        if(original_data) {
            FASTCPY(vector->data, original_data, new_byte_size);
            free(original_data);
        }

        vector->capacity = vector->size;
    }
}

void aligned_vector_cleanup(AlignedVector* vector) {
    aligned_vector_clear(vector);
    aligned_vector_shrink_to_fit(vector);
}
