#pragma once

#include <kos.h>
#include <dc/matrix.h>
#include <dc/pvr.h>
#include <dc/vec3f.h>
#include <dc/fmath.h>
#include <dc/matrix3d.h>

#include "../types.h"
#include "../private.h"

#ifndef NDEBUG
#define PERF_WARNING(msg) printf("[PERF] %s\n", msg)
#else
#define PERF_WARNING(msg) (void) 0
#endif

#ifndef GL_FORCE_INLINE
#define GL_NO_INSTRUMENT inline __attribute__((no_instrument_function))
#define GL_INLINE_DEBUG GL_NO_INSTRUMENT __attribute__((always_inline))
#define GL_FORCE_INLINE static GL_INLINE_DEBUG
#endif


// ---- sh4_math.h - SH7091 Math Module ----
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This math module is hereby released into the public domain in the hope that it
// may prove useful. Now go hit 60 fps! :)
//
// --Moopthehedgehog

// 1/sqrt(x)
GL_FORCE_INLINE float MATH_fsrra(float x)
{
  asm volatile ("fsrra %[one_div_sqrt]\n"
  : [one_div_sqrt] "+f" (x) // outputs, "+" means r/w
  : // no inputs
  : // no clobbers
  );

  return x;
}

// 1/x = 1 / sqrt(x^2)
GL_FORCE_INLINE float MATH_Fast_Invert(float x)
{
  int neg = x < 0.0f;

  x = MATH_fsrra(x * x);

  if (neg) x = -x;
  return x;
}
// end of ---- sh4_math.h ----

#define PREFETCH(addr) __builtin_prefetch((addr))

GL_FORCE_INLINE void* memcpy_fast(void *dest, const void *src, size_t len) {
  if(!len) {
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

/* We use sq_cpy if the src and size is properly aligned. We control that the
 * destination is properly aligned so we assert that. */
#define FASTCPY(dst, src, bytes) \
    do { \
        if(bytes % 32 == 0 && ((uintptr_t) src % 4) == 0) { \
            gl_assert(((uintptr_t) dst) % 32 == 0); \
            sq_cpy(dst, src, bytes); \
        } else { \
            memcpy_fast(dst, src, bytes); \
        } \
    } while(0)


#define MEMCPY4(dst, src, bytes) memcpy_fast(dst, src, bytes)

#define MEMSET4(dst, v, size) memset((dst), (v), (size))

#define VEC3_NORMALIZE(x, y, z) vec3f_normalize((x), (y), (z))
#define VEC3_LENGTH(x, y, z, l) vec3f_length((x), (y), (z), (l))
#define VEC3_DOT(x1, y1, z1, x2, y2, z2, d) vec3f_dot((x1), (y1), (z1), (x2), (y2), (z2), (d))

GL_FORCE_INLINE void UploadMatrix4x4(const Matrix4x4* mat) {
    mat_load((matrix_t*) mat);
}

GL_FORCE_INLINE void DownloadMatrix4x4(Matrix4x4* mat) {
    mat_store((matrix_t*) mat);
}

GL_FORCE_INLINE void MultiplyMatrix4x4(const Matrix4x4* mat) {
    mat_apply((matrix_t*) mat);
}

GL_FORCE_INLINE void TransformVec3(float* x) {
    mat_trans_single4(x[0], x[1], x[2], x[3]);
}

/* Transform a 3-element vector using the stored matrix (w == 1) */
GL_FORCE_INLINE void TransformVec3NoMod(const float* xIn, float* xOut) {
    mat_trans_single3_nodiv_nomod(xIn[0], xIn[1], xIn[2], xOut[0], xOut[1], xOut[2]);
}

/* Transform a 3-element normal using the stored matrix (w == 0)*/
GL_FORCE_INLINE void TransformNormalNoMod(const float* in, float* out) {
    mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);
}

/* Transform a 4-element vector in-place by the stored matrix */
inline void TransformVec4(float* x) {

}

GL_FORCE_INLINE void TransformVertex(float x, float y, float z, float w, float* oxyz, float* ow) {
    register float __x __asm__("fr4") = x;
    register float __y __asm__("fr5") = y;
    register float __z __asm__("fr6") = z;
    register float __w __asm__("fr7") = w;

    __asm__ __volatile__(
        "ftrv   xmtrx,fv4\n"
        : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w)
        : "0" (__x), "1" (__y), "2" (__z), "3" (__w)
    );

    oxyz[0] = __x;
    oxyz[1] = __y;
    oxyz[2] = __z;
    *ow = __w;
}

void InitGPU(_Bool autosort, _Bool fsaa);

static inline size_t GPUMemoryAvailable() {
    return pvr_mem_available();
}

static inline void* GPUMemoryAlloc(size_t size) {
    return pvr_mem_malloc(size);
}

static inline void GPUSetPaletteFormat(GPUPaletteFormat format) {
    pvr_set_pal_format(format);
}

static inline void GPUSetPaletteEntry(uint32_t idx, uint32_t value) {
    pvr_set_pal_entry(idx, value);
}

static inline void GPUSetBackgroundColour(float r, float g, float b) {
    pvr_set_bg_color(r, g, b);
}

#define PT_ALPHA_REF 0x011c

static inline void GPUSetAlphaCutOff(uint8_t val) {
    PVR_SET(PT_ALPHA_REF, val);
}

static inline void GPUSetClearDepth(float v) {
    pvr_set_zclip(v);
}

static inline void GPUSetFogLinear(float start, float end) {
    pvr_fog_table_linear(start, end);
}

static inline void GPUSetFogExp(float density) {
    pvr_fog_table_exp(density);
}

static inline void GPUSetFogExp2(float density) {
    pvr_fog_table_exp2(density);
}

static inline void GPUSetFogColor(float r, float g, float b, float a) {
    pvr_fog_table_color(r, g, b, a);
}
