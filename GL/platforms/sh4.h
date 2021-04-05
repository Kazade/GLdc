#pragma once

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
#define NORMALIZEVEC3(x, y, z) vec3f_normalize((x), (y), (z))

inline void CompilePolyHeader(PolyHeader* out, const PolyContext* in) {
    pvr_poly_compile((pvr_poly_hdr_t*) out, (pvr_poly_cxt_t*) in);
}

inline void UploadMatrix4x4(const float* mat) {
    mat_load((matrix_t*) mat);
}

inline void DownloadMatrix4x4(float* mat) {
    mat_store((matrix_t*) mat);
}

inline void MultiplyMatrix4x4(const float* mat) {
    mat_apply((matrix_t*) mat);
}

inline void TransformVertices(const Vertex* vertices, const int count) {
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
