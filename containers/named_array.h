#ifndef NAMED_ARRAY_H
#define NAMED_ARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int element_size;
    unsigned int max_element_count;
    unsigned char* elements;
    unsigned char* used_markers;
    unsigned char marker_count;
} NamedArray;

void named_array_init(NamedArray* array, unsigned int element_size, unsigned int max_elements);
char named_array_used(NamedArray* array, unsigned int id);

void* named_array_alloc(NamedArray* array, unsigned int* new_id);
void* named_array_reserve(NamedArray* array, unsigned int id);

void named_array_release(NamedArray* array, unsigned int new_id);
void* named_array_get(NamedArray* array, unsigned int id);
void named_array_cleanup(NamedArray* array);

#ifdef __cplusplus
}
#endif

#endif // NAMED_ARRAY_H
