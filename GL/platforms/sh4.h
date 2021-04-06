#pragma once

#include <kos.h>
#include <dc/matrix.h>
#include <dc/pvr.h>
#include <dc/vec3f.h>
#include <dc/fmath.h>
#include <dc/matrix3d.h>

#include "../types.h"
#include "sh4_math.h"

#define FASTCPY(dst, src, bytes) \
    (bytes % 32 == 0) ? sq_cpy(dst, src, bytes) : memcpy(dst, src, bytes)

#define FASTCPY4(dst, src, bytes) \
    (bytes % 32 == 0) ? sq_cpy(dst, src, bytes) : memcpy4(dst, src, bytes)

#define MEMSET4(dst, v, size) memset4((dst), (v), (size))

#define VEC3_NORMALIZE(x, y, z) vec3f_normalize((x), (y), (z))
#define VEC3_LENGTH(x, y, z, l) vec3f_length((x), (y), (z), (l))
#define VEC3_DOT(x1, y1, z1, x2, y2, z2, d) vec3f_dot((x1), (y1), (z1), (x2), (y2), (z2), (d))

static inline void UploadMatrix4x4(const Matrix4x4* mat) {
    mat_load((matrix_t*) mat);
}

static inline void DownloadMatrix4x4(Matrix4x4* mat) {
    mat_store((matrix_t*) mat);
}

static inline void MultiplyMatrix4x4(const Matrix4x4* mat) {
    mat_apply((matrix_t*) mat);
}

static inline void TransformVec3(float* x) {
    mat_trans_single4(x[0], x[1], x[2], x[3]);
}

/* Transform a 3-element vector using the stored matrix (w == 1) */
static inline void TransformVec3NoMod(const float* xIn, float* xOut) {
    mat_trans_single3_nodiv_nomod(xIn[0], xIn[1], xIn[2], xOut[0], xOut[1], xOut[2]);
}

/* Transform a 3-element normal using the stored matrix (w == 0)*/
static inline void TransformNormalNoMod(const float* in, float* out) {
    mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);
}

/* Transform a 4-element vector in-place by the stored matrix */
inline void TransformVec4(float* x) {

}

static inline void TransformVertices(Vertex* vertices, const int count) {
    Vertex* it = vertices;
    for(int i = 0; i < count; ++i, ++it) {
        register float __x __asm__("fr12") = (it->xyz[0]);
        register float __y __asm__("fr13") = (it->xyz[1]);
        register float __z __asm__("fr14") = (it->xyz[2]);
        register float __w __asm__("fr15") = (it->w);

        __asm__ __volatile__(
            "fldi1 fr15\n"
            "ftrv   xmtrx,fv12\n"
            : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w)
            : "0" (__x), "1" (__y), "2" (__z), "3" (__w)
        );

        it->xyz[0] = __x;
        it->xyz[1] = __y;
        it->xyz[2] = __z;
        it->w = __w;
    }
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
