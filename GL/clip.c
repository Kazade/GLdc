#include <float.h>
#include <stdio.h>

#ifdef _arch_dreamcast
#include <dc/pvr.h>
#else
#define PVR_PACK_COLOR(a, r, g, b) {}
#endif

#include "profiler.h"
#include "clip.h"
#include "../containers/aligned_vector.h"


static unsigned char ZCLIP_ENABLED = 1;

unsigned char isClippingEnabled() {
    return ZCLIP_ENABLED;
}

void enableClipping(unsigned char v) {
    ZCLIP_ENABLED = v;
}

void clipLineToNearZ(const ClipVertex* v1, const ClipVertex* v2, ClipVertex* vout, float* t) __attribute__((optimize("fast-math")));
void clipLineToNearZ(const ClipVertex* v1, const ClipVertex* v2, ClipVertex* vout, float* t) {
    const float NEAR_PLANE = 0.2; // FIXME: this needs to be read from the projection matrix.. somehow

    *t = (NEAR_PLANE - v1->w) / (v2->w - v1->w);

    float vec [] = {v2->xyz[0] - v1->xyz[0], v2->xyz[1] - v1->xyz[1], v2->xyz[2] - v1->xyz[2]};

    vout->xyz[0] = v1->xyz[0] + (vec[0] * (*t));
    vout->xyz[1] = v1->xyz[1] + (vec[1] * (*t));
    vout->xyz[2] = v1->xyz[2] + (vec[2] * (*t));
}

static inline void interpolateFloat(const float v1, const float v2, const float t, float* out) {
    float v = v2 - v1;
    *out = (v * t) + v1;
}

static inline void interpolateVec2(const float* v1, const float* v2, const float t, float* out) {
    /* FIXME: SH4 has an asm instruction for this */
    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
}

static inline void interpolateVec3(const float* v1, const float* v2, const float t, float* out) {
    /* FIXME: SH4 has an asm instruction for this */

    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
    interpolateFloat(v1[2], v2[2], t, &out[2]);
}

static inline void interpolateVec4(const float* v1, const float* v2, const float t, float* out) {
    /* FIXME: SH4 has an asm instruction for this */
    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
    interpolateFloat(v1[2], v2[2], t, &out[2]);
    interpolateFloat(v1[3], v2[3], t, &out[3]);
}

const uint32_t VERTEX_CMD_EOL = 0xf0000000;
const uint32_t VERTEX_CMD = 0xe0000000;

void clipTriangleStrip(AlignedVector* vertices, AlignedVector* outBuffer) __attribute__((optimize("fast-math")));
void clipTriangleStrip(AlignedVector* vertices, AlignedVector* outBuffer) {

    /* Clipping triangle strips is *hard* this is the algorithm we follow:
     *
     * - Treat each triangle in the strip individually.
     * - If we find a triangle that needs clipping, treat it in isolation.
     *   - End the strip at the triangle
     *   - Generate a new single-triangle strip for it
     *   - Begin a new strip for the remainder of the strip
     *
     * There is probably more efficient way but there are so many different cases to handle that it's
     * difficult to even write them down!
     */

    uint32_t i;
    uint32_t stripCount = 2; /* The number of vertices in the source strip so far */

    ClipVertex* thisVertex = aligned_vector_at(vertices, 1);

    for(i = 2; i < vertices->size; ++i) {
        ++thisVertex;

        if(stripCount < 2) {
            stripCount++;
            continue;
        }

        const ClipVertex* sourceTriangle[3] = {
            thisVertex - 2,
            thisVertex - 1,
            thisVertex
        };

        /* If we're on an odd vertex, we need to swap the order of the first two vertices, as that's what
         * triangle strips do */
        uint32_t swap = stripCount > 2 && (stripCount % 2 != 0);
        const ClipVertex* v1 = swap ? sourceTriangle[1] : sourceTriangle[0];
        const ClipVertex* v2 = swap ? sourceTriangle[0] : sourceTriangle[1];
        const ClipVertex* v3 = sourceTriangle[2];

        uint32_t visible = ((v1->w > 0) ? 4 : 0) | ((v2->w > 0) ? 2 : 0) | ((v3->w > 0) ? 1 : 0);
        uint32_t startOfStrip = (i == 2) || (outBuffer->size > 2 && ((ClipVertex*) aligned_vector_back(outBuffer))->flags == VERTEX_CMD_EOL);

        /* All visible, we're fine! */
        if(visible == 0b111) {
            if(startOfStrip) {
                aligned_vector_push_back(outBuffer, v1, 1);
                aligned_vector_push_back(outBuffer, v2, 1);
            }

            aligned_vector_push_back(outBuffer, v3, 1);
        } else if(visible == 0b000) {
            /* Do nothing */
            continue;
        } else if(visible == 0b100) {
            /* Only the first vertex is visible */
            float t1 = 0, t2 = 0;

            ClipVertex output[3];

            clipLineToNearZ(v1, v2, &output[1], &t1);
            clipLineToNearZ(v1, v3, &output[2], &t2);

            interpolateFloat(v1->w, v2->w, t1, &output[1].w);
            interpolateFloat(v1->w, v3->w, t2, &output[2].w);

            output[0] = *v1;

            /* Interpolate normals */
            interpolateVec3(v1->nxyz, v2->nxyz, t1, output[1].nxyz);
            interpolateVec3(v1->nxyz, v3->nxyz, t2, output[2].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v1->uv, v2->uv, t1, output[1].uv);
            interpolateVec2(v1->uv, v3->uv, t2, output[2].uv);

            interpolateVec2(v1->st, v2->st, t1, output[1].st);
            interpolateVec2(v1->st, v3->st, t2, output[2].st);

            interpolateVec4(v1->diffuse, v2->diffuse, t1, output[1].diffuse);
            interpolateVec4(v1->diffuse, v3->diffuse, t2, output[2].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 3);
        } else if(visible == 0b010) {
            /* Only the second vertex is visible */

            float t1 = 0, t2 = 0;

            ClipVertex output[3];

            clipLineToNearZ(v2, v1, &output[0], &t1);
            clipLineToNearZ(v2, v3, &output[2], &t2);

            interpolateFloat(v2->w, v1->w, t1, &output[0].w);
            interpolateFloat(v2->w, v3->w, t2, &output[2].w);

            output[1] = *v2;

            /* Interpolate normals */
            interpolateVec3(v2->nxyz, v1->nxyz, t1, output[0].nxyz);
            interpolateVec3(v2->nxyz, v3->nxyz, t2, output[2].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v2->uv, v1->uv, t1, output[0].uv);
            interpolateVec2(v2->uv, v3->uv, t2, output[2].uv);

            interpolateVec2(v2->st, v1->st, t1, output[0].st);
            interpolateVec2(v2->st, v3->st, t2, output[2].st);

            interpolateVec4(v2->diffuse, v1->diffuse, t1, output[0].diffuse);
            interpolateVec4(v2->diffuse, v3->diffuse, t2, output[2].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 3);
        } else if(visible == 0b001) {
            /* Only the third vertex is visible */

            float t1 = 0, t2 = 0;

            ClipVertex output[3];

            clipLineToNearZ(v3, v1, &output[0], &t1);
            clipLineToNearZ(v3, v2, &output[1], &t2);

            interpolateFloat(v3->w, v1->w, t1, &output[0].w);
            interpolateFloat(v3->w, v2->w, t2, &output[1].w);

            output[2] = *v3;

            /* Interpolate normals */
            interpolateVec3(v3->nxyz, v1->nxyz, t1, output[0].nxyz);
            interpolateVec3(v3->nxyz, v2->nxyz, t2, output[1].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v3->uv, v1->uv, t1, output[0].uv);
            interpolateVec2(v3->uv, v2->uv, t2, output[1].uv);

            interpolateVec2(v3->st, v1->st, t1, output[0].st);
            interpolateVec2(v3->st, v2->st, t2, output[1].st);

            interpolateVec4(v3->diffuse, v1->diffuse, t1, output[0].diffuse);
            interpolateVec4(v3->diffuse, v2->diffuse, t2, output[1].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 3);
        } else if(visible == 0b110) {
            /* Third vertex isn't visible */

            float t1 = 0, t2 = 0;

            ClipVertex output[4];

            clipLineToNearZ(v2, v3, &output[2], &t1);
            clipLineToNearZ(v1, v3, &output[3], &t2);

            interpolateFloat(v2->w, v3->w, t1, &output[2].w);
            interpolateFloat(v1->w, v3->w, t2, &output[3].w);

            output[0] = *v1;
            output[1] = *v2;

            /* Interpolate normals */
            interpolateVec3(v2->nxyz, v3->nxyz, t1, output[2].nxyz);
            interpolateVec3(v1->nxyz, v3->nxyz, t2, output[3].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v2->uv, v3->uv, t1, output[2].uv);
            interpolateVec2(v1->uv, v3->uv, t2, output[3].uv);

            interpolateVec2(v2->st, v3->st, t1, output[2].st);
            interpolateVec2(v1->st, v3->st, t2, output[3].st);

            interpolateVec4(v2->diffuse, v3->diffuse, t1, output[2].diffuse);
            interpolateVec4(v1->diffuse, v3->diffuse, t2, output[3].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD;
            output[3].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 4);
        } else if(visible == 0b011) {
            /* First vertex isn't visible, so let's clip along the lines to the second and third */
            float t1 = 0, t2 = 0;

            ClipVertex output[4];

            clipLineToNearZ(v1, v2, &output[0], &t1);
            clipLineToNearZ(v1, v3, &output[2], &t2);

            interpolateFloat(v1->w, v2->w, t1, &output[0].w);
            interpolateFloat(v1->w, v3->w, t2, &output[2].w);

            output[1] = *v2;
            output[3] = *v3;

            /* Interpolate normals */
            interpolateVec3(v1->nxyz, v2->nxyz, t1, output[0].nxyz);
            interpolateVec3(v1->nxyz, v3->nxyz, t2, output[2].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v1->uv, v2->uv, t1, output[0].uv);
            interpolateVec2(v1->uv, v3->uv, t2, output[2].uv);

            interpolateVec2(v1->st, v2->st, t1, output[0].st);
            interpolateVec2(v1->st, v3->st, t2, output[2].st);

            interpolateVec4(v1->diffuse, v2->diffuse, t1, output[0].diffuse);
            interpolateVec4(v1->diffuse, v3->diffuse, t2, output[2].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD;
            output[3].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 4);
        } else if(visible == 0b101) {
            /* Second vertex isn't visible */
            float t1 = 0, t2 = 0;

            ClipVertex output[4];

            clipLineToNearZ(v1, v2, &output[1], &t1);
            clipLineToNearZ(v3, v2, &output[3], &t2);

            interpolateFloat(v1->w, v2->w, t1, &output[1].w);
            interpolateFloat(v3->w, v2->w, t2, &output[3].w);

            output[0] = *v1;
            output[2] = *v3;

            /* Interpolate normals */
            interpolateVec3(v1->nxyz, v2->nxyz, t1, output[1].nxyz);
            interpolateVec3(v3->nxyz, v2->nxyz, t2, output[3].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v1->uv, v2->uv, t1, output[1].uv);
            interpolateVec2(v3->uv, v2->uv, t2, output[3].uv);

            interpolateVec2(v1->st, v2->st, t1, output[1].st);
            interpolateVec2(v3->st, v2->st, t2, output[3].st);

            interpolateVec4(v1->diffuse, v2->diffuse, t1, output[1].diffuse);
            interpolateVec4(v3->diffuse, v2->diffuse, t2, output[3].diffuse);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD;
            output[3].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 4);
        }

        /* If this vertex was the last in the list, reset the stripCount */
        if(thisVertex->flags == VERTEX_CMD_EOL) {
            stripCount = 0;
        } else {
            stripCount++;
        }

    }
}
