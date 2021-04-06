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

#define VEC3_NORMALIZE(x, y, z) \
    do { \
        float l = MATH_fsrra((x) * (x) + (y) * (y) + (z) * (z)); \
        x *= l; \
        y *= l; \
        z *= l; \
    } while(0)

#define VEC3_LENGTH(x, y, z, d) \
    d = MATH_Fast_Sqrt((x) * (x) + (y) * (y) + (z) * (z))

#define VEC3_DOT(x1, y1, z1, x2, y2, z2, d) \
    d = (x1 * x2) + (y1 * y2) + (z1 * z2)

struct PolyHeader;
struct PolyContext;

static inline void CompilePolyHeader(PolyHeader* out, const PolyContext* in) {
    (void) out;
    (void) in;
}

void UploadMatrix4x4(const Matrix4x4* mat);
void MultiplyMatrix4x4(const Matrix4x4* mat);
void DownloadMatrix4x4(Matrix4x4* mat);

/* Transform a 3-element vector in-place using the stored matrix (w == 1) */
static inline void TransformVec3(float* x) {

}

/* Transform a 3-element vector using the stored matrix (w == 1) */
static inline void TransformVec3NoMod(const float* xIn, float* xOut) {

}

/* Transform a 3-element normal using the stored matrix (w == 0)*/
static inline void TransformNormalNoMod(const float* xIn, float* xOut) {

}

/* Transform a 4-element vector in-place by the stored matrix */
static inline void TransformVec4(float* x) {

}

static inline void TransformVertices(Vertex* vertices, const int count) {
    (void) vertices;
    (void) count;
}

void InitGPU(_Bool autosort, _Bool fsaa);

enum GPUPaletteFormat;

size_t GPUMemoryAvailable();
void* GPUMemoryAlloc(size_t size);
void GPUSetPaletteFormat(GPUPaletteFormat format);
void GPUSetPaletteEntry(uint32_t idx, uint32_t value);

void GPUSetBackgroundColour(float r, float g, float b);
void GPUSetAlphaCutOff(uint8_t v);
void GPUSetClearDepth(float v);

void GPUSetFogLinear(float start, float end);
void GPUSetFogExp(float density);
void GPUSetFogExp2(float density);
void GPUSetFogColor(float r, float g, float b, float a);

