#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __APPLE__
#include <malloc.h>
#else
/* Linux + Kos define this, OSX does not, so just use malloc there */
#define memalign(x, size) malloc((size))
#endif

#include "aligned_vector.h"

void aligned_vector_init(AlignedVector* vector, unsigned int element_size) {
    vector->size = vector->capacity = 0;
    vector->element_size = element_size;
    vector->data = NULL;

    /* Reserve some initial capacity */
    aligned_vector_reserve(vector, ALIGNED_VECTOR_INITIAL_CAPACITY);
}

void aligned_vector_reserve(AlignedVector* vector, unsigned int element_count) {
    if(element_count <= vector->capacity) {
        return;
    }

    unsigned int original_byte_size = vector->size * vector->element_size;
    unsigned int new_byte_size = element_count * vector->element_size;
    unsigned char* original_data = vector->data;
    vector->data = (unsigned char*) memalign(0x20, new_byte_size);

    if(original_data) {
        memcpy(vector->data, original_data, original_byte_size);
        free(original_data);
    }

    vector->capacity = element_count;
}

void aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count) {
    /* Resize enough room */

    unsigned int initial_size = vector->size;
    aligned_vector_resize(vector, vector->size + count);

    unsigned char* dest = vector->data + (vector->element_size * initial_size);

    /* Copy the objects in */
    memcpy(dest, objs, vector->element_size * count);
}

void* aligned_vector_resize(AlignedVector* vector, const unsigned int element_count) {
    unsigned int previousCount = vector->size;

    /* Don't change memory when resizing downwards, just change the size */
    if(element_count <= vector->size) {
        vector->size = element_count;
        return NULL;
    }

    if(vector->capacity < element_count) {
        /* Reserve more than we need so that a subsequent push_back doesn't trigger yet another
         * resize */
        aligned_vector_reserve(vector, (int) ceil(((float)element_count) * 1.5f));
    }

    vector->size = element_count;

    if(previousCount < vector->size) {
        return aligned_vector_at(vector, previousCount);
    } else {
        return NULL;
    }
}

void* aligned_vector_at(const AlignedVector* vector, const unsigned int index) {
    return &vector->data[index * vector->element_size];
}

void* aligned_vector_back(AlignedVector* vector) {
    return aligned_vector_at(vector, vector->size - 1);
}

void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count) {
    const unsigned int current = vector->size;
    aligned_vector_resize(vector, vector->size + additional_count);
    return aligned_vector_at(vector, current);
}

void aligned_vector_clear(AlignedVector* vector) {
    vector->size = 0;
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
            memcpy(vector->data, original_data, new_byte_size);
            free(original_data);
        }

        vector->capacity = vector->size;
    }
}

void aligned_vector_cleanup(AlignedVector* vector) {
    aligned_vector_clear(vector);
    aligned_vector_shrink_to_fit(vector);
}
