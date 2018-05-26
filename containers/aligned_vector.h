#ifndef ALIGNED_VECTOR_H
#define ALIGNED_VECTOR_H

typedef struct {
    unsigned int size;
    unsigned int capacity;
    unsigned char* data;
    unsigned int element_size;
} AlignedVector;

void aligned_vector_init(AlignedVector* vector, unsigned int element_size);
void aligned_vector_reserve(AlignedVector* vector, unsigned int element_count);
void aligned_vector_push_back(AlignedVector* vector, const void* objs, unsigned int count);
void aligned_vector_resize(AlignedVector* vector, const unsigned int element_count);
void* aligned_vector_at(AlignedVector* vector, const unsigned int index);
void* aligned_vector_extend(AlignedVector* vector, const unsigned int additional_count);
void aligned_vector_clear(AlignedVector* vector);
void aligned_vector_shrink_to_fit(AlignedVector* vector);
void aligned_vector_cleanup(AlignedVector* vector);

#endif // ALIGNED_VECTOR_H
