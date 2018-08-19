#ifndef CLIP_H
#define CLIP_H

#include <stdint.h>

#include "../containers/aligned_vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLIP_RESULT_ALL_IN_FRONT,
    CLIP_RESULT_ALL_BEHIND,
    CLIP_RESULT_ALL_ON_PLANE,
    CLIP_RESULT_FRONT_TO_BACK,
    CLIP_RESULT_BACK_TO_FRONT
} ClipResult;


typedef struct {
    uint32_t flags;
    float xyz[3];
    float uv[2];

    float nxyz[3]; /* Normal */
    float w;

    float diffuse[4]; /* Colour in floating point */
    float st[2];
} ClipVertex;

void clipLineToNearZ(const ClipVertex* v1, const ClipVertex* v2, ClipVertex* vout, float* t);
void clipTriangleStrip(AlignedVector* vertices, AlignedVector* outBuffer);

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
