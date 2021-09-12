#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#ifdef _arch_dreamcast
#include "../GL/private.h"
#else
#define FASTCPY memcpy
#endif

#include "aligned_vector.h"

extern inline void* aligned_vector_resize(AlignedVector* vector, const unsigned int element_count);
extern inline void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count);
extern inline void* aligned_vector_reserve(AlignedVector* vector, unsigned int element_count);
extern inline void* aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count);

void aligned_vector_init(AlignedVector* vector, unsigned int element_size) {
    vector->size = vector->capacity = 0;
    vector->element_size = element_size;
    vector->data = NULL;

    /* Reserve some initial capacity */
    aligned_vector_reserve(vector, ALIGNED_VECTOR_CHUNK_SIZE);
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
