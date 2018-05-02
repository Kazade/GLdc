#ifndef NAMED_ARRAY_H
#define NAMED_ARRAY_H

#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    unsigned int element_size;
    unsigned int max_element_count;
    unsigned char* elements;
    unsigned char* used_markers;
    unsigned char marker_count;
} NamedArray;


inline void named_array_init(NamedArray* array, unsigned int element_size, unsigned int max_elements) {
    array->element_size = element_size;
    array->max_element_count = max_elements;

    float c = (float) max_elements / 8.0f;
    array->marker_count = (unsigned char) ceil(c);

#ifdef _arch_dreamcast
    // Use 32-bit aligned memory on the Dreamcast
    array->elements = (unsigned char*) memalign(0x20, element_size * max_elements);
    array->used_markers = (unsigned char*) memalign(0x20, array->marker_count);
#else
    array->elements = (unsigned char*) malloc(element_size * max_elements);
    array->used_markers = (unsigned char*) malloc(array->marker_count);
#endif
    memset(array->used_markers, 0, sizeof(array->marker_count));
}

inline char named_array_used(NamedArray* array, unsigned int id) {
    id--;

    unsigned int i = id / 8;
    unsigned int j = id % 8;

    unsigned char v = array->used_markers[i] & (unsigned char) (1 << j);
    return !!(v);
}

inline void* named_array_alloc(NamedArray* array, unsigned int* new_id) {
    for(unsigned int i = 0; i < array->marker_count; ++i) {
        for(unsigned int j = 0; j < 8; ++j) {
            unsigned int id = (i * 8) + j + 1;
            if(!named_array_used(array, id)) {
                array->used_markers[i] |= (unsigned char) 1 << j;
                *new_id = id;
                unsigned char* ptr = &array->elements[(id - 1) * array->element_size];
                memset(ptr, 0, array->element_size);
                return ptr;
            }
        }
    }

    return NULL;
}

inline void named_array_release(NamedArray* array, unsigned int new_id) {
    new_id--;

    unsigned int i = new_id / 8;
    unsigned int j = new_id % 8;

    array->used_markers[i] &= (unsigned char) ~(1 << j);
}

inline void* named_array_get(NamedArray* array, unsigned int id) {
    if(id == 0) {
        return NULL;
    }

    if(!named_array_used(array, id)) {
        return NULL;
    }

    return &array->elements[(id - 1) * array->element_size];
}

inline void named_array_cleanup(NamedArray* array) {
    free(array->elements);
    free(array->used_markers);
    array->elements = NULL;
    array->used_markers = NULL;
    array->element_size = array->max_element_count = 0;
    array->marker_count = 0;
}


#endif // NAMED_ARRAY_H
