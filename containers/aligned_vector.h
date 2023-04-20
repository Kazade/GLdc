#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__) || defined(__WIN32__)
/* Linux + Kos define this, OSX does not, so just use malloc there */
static inline void* memalign(size_t alignment, size_t size) {
    (void) alignment;
    return malloc(size);
}
#else
    #include <malloc.h>
#endif

#ifdef __cplusplus
#define AV_FORCE_INLINE static inline
#else
#define AV_NO_INSTRUMENT inline __attribute__((no_instrument_function))
#define AV_INLINE_DEBUG AV_NO_INSTRUMENT __attribute__((always_inline))
#define AV_FORCE_INLINE static AV_INLINE_DEBUG
#endif


#ifdef __DREAMCAST__
#include <kos/string.h>

AV_FORCE_INLINE void *AV_MEMCPY4(void *dest, const void *src, size_t len)
{
  if(!len)
  {
    return dest;
  }

  const uint8_t *s = (uint8_t *)src;
  uint8_t *d = (uint8_t *)dest;

  uint32_t diff = (uint32_t)d - (uint32_t)(s + 1); // extra offset because input gets incremented before output is calculated
  // Underflow would be like adding a negative offset

  // Can use 'd' as a scratch reg now
  asm volatile (
    "clrs\n" // Align for parallelism (CO) - SH4a use "stc SR, Rn" instead with a dummy Rn
  ".align 2\n"
  "0:\n\t"
    "dt %[size]\n\t" // (--len) ? 0 -> T : 1 -> T (EX 1)
    "mov.b @%[in]+, %[scratch]\n\t" // scratch = *(s++) (LS 1/2)
    "bf.s 0b\n\t" // while(s != nexts) aka while(!T) (BR 1/2)
    " mov.b %[scratch], @(%[offset], %[in])\n" // *(datatype_of_s*) ((char*)s + diff) = scratch, where src + diff = dest (LS 1)
    : [in] "+&r" ((uint32_t)s), [scratch] "=&r" ((uint32_t)d), [size] "+&r" (len) // outputs
    : [offset] "z" (diff) // inputs
    : "t", "memory" // clobbers
  );

  return dest;
}

#else
#define AV_MEMCPY4 memcpy
#endif

typedef struct {
    uint8_t* __attribute__((aligned(32))) data;
    uint32_t size;
    uint32_t capacity;
    uint32_t element_size;
} AlignedVector;

#define ALIGNED_VECTOR_CHUNK_SIZE 256u


#define ROUND_TO_CHUNK_SIZE(v) \
    ((((v) + ALIGNED_VECTOR_CHUNK_SIZE - 1) / ALIGNED_VECTOR_CHUNK_SIZE) * ALIGNED_VECTOR_CHUNK_SIZE)


void aligned_vector_init(AlignedVector* vector, unsigned int element_size);

AV_FORCE_INLINE void* aligned_vector_reserve(AlignedVector* vector, unsigned int element_count) {
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
        AV_MEMCPY4(vector->data, original_data, original_byte_size);
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

    if(vector->capacity < element_count) {
        /* If we didn't have capacity, increase capacity (slow) */
        vector->size = element_count;
        ret = aligned_vector_reserve(vector, element_count);
    } else if(previousCount < element_count) {
        /* So we grew, but had the capacity, just get a pointer to
         * where we were */
        vector->size = element_count;
        ret = aligned_vector_at(vector, previousCount);
    } else {
        vector->size = element_count;
    }

    return ret;
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
    AV_MEMCPY4(dest, objs, vector->element_size * count);

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
