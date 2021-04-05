#pragma once

#include <math.h>
#include <memory.h>

#include "../types.h"

#define MATH_Fast_Divide(n, d) (n / d)
#define MATH_fmac(a, b, c) (a * b + c)
#define MATH_Fast_Sqrt(x) sqrtf((x))
#define MATH_fsrra(x) (1.0f / sqrtf((x)))
#define MATH_Fast_Invert(x) (1.0f / (x))

#define FASTCPY(dst, src, bytes) memcpy(dst, src, bytes)
#define FASTCPY4(dst, src, bytes) memcpy(dst, src, bytes)
#define MEMSET4(dst, v, size) memset((dst), (v), (size))
#define NORMALIZEVEC3(x, y, z) \
    do { \
        float l = MATH_fsrra((x) * (x) + (y) * (y) + (z) * (z)); \
        x *= l; \
        y *= l; \
        z *= l; \
    while(0); \

struct PolyHeader;
struct PolyContext;

inline void CompilePolyHeader(PolyHeader* out, const PolyContext* in) {
    (void) out;
    (void) in;
}

inline void UploadMatrix4x4(const Matrix4x4* mat) {
    (void) mat;
}

inline void MultiplyMatrix4x4(const Matrix4x4* mat) {
    (void) mat;
}

inline void DownloadMatrix4x4(Matrix4x4* mat) {
    (void) mat;
}

inline void TransformVertices(const Vertex* vertices, const int count) {
    (void) vertices;
    (void) count;
}

void InitGPU(_Bool autosort, _Bool fsaa);