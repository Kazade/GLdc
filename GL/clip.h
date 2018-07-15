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
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ClipColour;

/* Note: This structure is the almost the same format as pvr_vertex_t aside from the offet
 * (oargb) which is replaced by the floating point w value. This is so that we can
 * simply zero it and memcpy the lot into the output */
typedef struct {
    uint32_t flags;
    float xyz[3];
    float uv[2];
    ClipColour argb;
    float nxyz[3];
    float w;

    float xyzES[3]; /* Coordinate in eye space */
    float nES[3]; /* Normal in eye space */
} ClipVertex;

ClipResult clipLineToNearZ(const float* v1, const float* v2, const float dist, float* vout, float* t);


void clipTriangleStrip(AlignedVector* vertices, AlignedVector* outBuffer);


/* FIXME: Why this value? This was copied from libGL because using zero wasn't working right.
 *
 * I think this is a hack. We should really be clipping against the W coordinate of each vertex... but I'm not sure
 * how yet.. this is probably related to the way the z coordinate is mapped to window coordinates or something
 */
#define CLIP_DISTANCE -0.2

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
