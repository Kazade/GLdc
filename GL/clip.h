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


#define A8IDX 3
#define R8IDX 2
#define G8IDX 1
#define B8IDX 0


typedef struct {
    /* Same 32 byte layout as pvr_vertex_t */
    uint32_t flags;
    float xyz[3];
    float uv[2];
    uint8_t bgra[4];
    uint32_t oargb;

    /* Important, we have 24 bytes here. That means when submitting to the SQs we need to
     * increment the pointer by 6 */
    float nxyz[3]; /* Normal */
    float w;
    float st[2];
} ClipVertex;

void clipLineToNearZ(const ClipVertex* v1, const ClipVertex* v2, ClipVertex* vout, float* t);
void clipTriangleStrip(const ClipVertex* vertices, const unsigned int count, AlignedVector* outBuffer);

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
