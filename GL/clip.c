#include <float.h>
#include <stdio.h>

#ifdef _arch_dreamcast
#include <dc/pvr.h>
#else
#define PVR_PACK_COLOR(a, r, g, b) {}
#endif

#include "clip.h"
#include "../containers/aligned_vector.h"


void clipLineToNearZ(const ClipVertex* v1, const ClipVertex* v2, ClipVertex* vout, float* t) {
    const float NEAR_PLANE = 0.2; // FIXME: this needs to be read from the projection matrix.. somehow

    *t = (NEAR_PLANE - v1->w) / (v2->w - v1->w);

    float vec [] = {v2->xyz[0] - v1->xyz[0], v2->xyz[1] - v1->xyz[1], v2->xyz[2] - v1->xyz[2]};

    vout->xyz[0] = v1->xyz[0] + (vec[0] * (*t));
    vout->xyz[1] = v1->xyz[1] + (vec[1] * (*t));
    vout->xyz[2] = v1->xyz[2] + (vec[2] * (*t));
}

static void interpolateFloat(const float v1, const float v2, const float t, float* out) {
    *out = v1 + (v2 - v1) * t;
}

static void interpolateVec2(const float* v1, const float* v2, const float t, float* out) {
    /* FIXME: SH4 has an asm instruction for this */
    out[0] = v1[0] + (v2[0] - v1[0]) * t;
    out[1] = v1[1] + (v2[1] - v1[1]) * t;
}

static void interpolateVec3(const float* v1, const float* v2, const float t, float* out) {
    /* FIXME: SH4 has an asm instruction for this */
    out[0] = v1[0] + (v2[0] - v1[0]) * t;
    out[1] = v1[1] + (v2[1] - v1[1]) * t;
    out[2] = v1[2] + (v2[2] - v1[2]) * t;
}

static void interpolateColour(const uint32_t* c1, const uint32_t* c2, const float t, uint32_t* out) {
    float r1 = (*c1 >> 16) & 0xFF;
    float r2 = (*c2 >> 16) & 0xFF;
    uint8_t r = (r1 + (r2 - r1) * t);

    r1 = (*c1 >> 24) & 0xFF;
    r2 = (*c2 >> 24) & 0xFF;
    uint8_t a = (r1 + (r2 - r1) * t);

    r1 = (*c1 >> 8) & 0xFF;
    r2 = (*c2 >> 8) & 0xFF;
    uint8_t g = (r1 + (r2 - r1) * t);

    r1 = (*c1 >> 0) & 0xFF;
    r2 = (*c2 >> 0) & 0xFF;
    uint8_t b = (r1 + (r2 - r1) * t);

    *out = (a << 24 | r << 16 | g << 8 | b);
}

const uint32_t VERTEX_CMD_EOL = 0xf0000000;
const uint32_t VERTEX_CMD = 0xe0000000;

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

    for(i = 2; i < vertices->size; ++i) {
        ClipVertex* sourceTriangle[3] = {
            aligned_vector_at(vertices, i - 2),
            aligned_vector_at(vertices, i - 1),
            aligned_vector_at(vertices, i)
        };

        /* If we're on an odd vertex, we need to swap the order of the first two vertices, as that's what
         * triangle strips do */
        uint8_t even = i % 2 == 0;
        ClipVertex* v1 = even ? sourceTriangle[0] : sourceTriangle[1];
        ClipVertex* v2 = even ? sourceTriangle[1] : sourceTriangle[0];
        ClipVertex* v3 = sourceTriangle[2];

        uint8_t visible = ((v1->w > 0) ? 4 : 0) | ((v2->w > 0) ? 2 : 0) | ((v3->w > 0) ? 1 : 0);
        uint8_t startOfStrip = (i == 2) || (outBuffer->size > 2 && ((ClipVertex*) aligned_vector_back(outBuffer))->flags == VERTEX_CMD_EOL);

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

            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v2->argb, t1, (uint32_t*) &output[1].argb);
            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v3->argb, t2, (uint32_t*) &output[2].argb);

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

            interpolateColour((uint32_t*) &v2->argb, (uint32_t*) &v1->argb, t1, (uint32_t*) &output[0].argb);
            interpolateColour((uint32_t*) &v2->argb, (uint32_t*) &v3->argb, t2, (uint32_t*) &output[2].argb);

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

            interpolateColour((uint32_t*) &v3->argb, (uint32_t*) &v1->argb, t1, (uint32_t*) &output[0].argb);
            interpolateColour((uint32_t*) &v3->argb, (uint32_t*) &v2->argb, t2, (uint32_t*) &output[1].argb);

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

            interpolateColour((uint32_t*) &v2->argb, (uint32_t*) &v3->argb, t1, (uint32_t*) &output[2].argb);
            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v3->argb, t2, (uint32_t*) &output[3].argb);

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

            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v2->argb, t1, (uint32_t*) &output[0].argb);
            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v3->argb, t2, (uint32_t*) &output[2].argb);

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

            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v2->argb, t1, (uint32_t*) &output[1].argb);
            interpolateColour((uint32_t*) &v3->argb, (uint32_t*) &v2->argb, t2, (uint32_t*) &output[3].argb);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD;
            output[3].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 4);
        }
    }
}
