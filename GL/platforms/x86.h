#pragma once

#include <math.h>
#include "../types.h"

#define MATH_Fast_Divide(n, d) (n / d)
#define MATH_fmac(a, b, c) (a * b + c)
#define MATH_Fast_Sqrt(x) sqrt((x))
#define MATH_fsrra(x) (1.0f / sqrt((x)))
#define MATH_Fast_Invert(x) (1.0f / (x))

inline void CompilePolyHeader(PolyHeader* out, const PolyContext* in) {
    (void) out;
    (void) in;
}

inline void UploadMatrix4x4(const float* mat) {
    (void) mat;
}

inline void MultiplyMatrix4x4(const float* mat) {
    (void) mat;
}

inline void DownloadMatrix4x4(float* mat) {
    (void) mat;
}

inline void TransformVertices(const Vertex* vertices, const int count) {
    (void) vertices;
    (void) count;
}
