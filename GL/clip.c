#include <float.h>
#include "clip.h"
#include "../containers/aligned_vector.h"

ClipResult clipLineToNearZ(const float* v1, const float* v2, const float dist, float* vout, float* t) {
    if(v1[2] < dist && v2[2] < dist) {
        // Both behind, no clipping
        return CLIP_RESULT_ALL_BEHIND;
    }

    if(v1[2] > dist && v2[2] > dist) {
        return CLIP_RESULT_ALL_IN_FRONT;
    }

    float vec [] = {v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2]};

    /*
     * The plane normal will always be pointing down the negative Z so we can simplify the dot products as x and y will always be zero
     * the resulting calculation will result in simply -z of the vector
    */
    float vecDotP = -vec[2];

    /* If the dot product is zero there is no intersection */
    if(vecDotP > FLT_MIN || vecDotP < -FLT_MIN) {
        *t = (-(dist - v1[2])) / vecDotP;

        vout[0] = v1[0] + (vec[0] * (*t));
        vout[1] = v1[1] + (vec[1] * (*t));
        vout[2] = v1[2] + (vec[2] * (*t));

        return (v1[2] >= dist) ? CLIP_RESULT_FRONT_TO_BACK : CLIP_RESULT_BACK_TO_FRONT;
    } else {
        return CLIP_RESULT_ALL_ON_PLANE;
    }
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

static void interpolateColour(const uint32_t* c1, const uint32_t* c2, const float t, const uint32_t* out) {
    /* FIXME: Needs float casting stuff to actually work */

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
        ClipVertex* v1 = (i % 2 == 0) ? sourceTriangle[0] : sourceTriangle[1];
        ClipVertex* v2 = (i % 2 == 0) ? sourceTriangle[1] : sourceTriangle[0];
        ClipVertex* v3 = sourceTriangle[2];

        uint8_t visible = ((v1->xyz[2] <= 0) ? 4 : 0) | ((v2->xyz[2] <= 0) ? 2 : 0) | ((v3->xyz[2] <= 0) ? 1 : 0);
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

        } else if(visible == 0b010) {

        } else if(visible == 0b001) {

        } else if(visible == 0b110) {
            /* Third vertex isn't visible */

            float t1 = 0, t2 = 0;

            ClipVertex output[4];

            /* FIXME: Interpolate other values (colors etc.) */
            clipLineToNearZ(v2->xyz, v3->xyz, 0, output[2].xyz, &t1);
            clipLineToNearZ(v1->xyz, v3->xyz, 0, output[3].xyz, &t2);

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

            /* FIXME: Interpolate other values (colors etc.) */
            clipLineToNearZ(v1->xyz, v2->xyz, 0, output[0].xyz, &t1);
            clipLineToNearZ(v1->xyz, v3->xyz, 0, output[2].xyz, &t2);

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

            /* FIXME: Interpolate other values (colors etc.) */
            clipLineToNearZ(v1->xyz, v2->xyz, 0, output[1].xyz, &t1);
            clipLineToNearZ(v3->xyz, v2->xyz, 0, output[2].xyz, &t2);

            output[0] = *v1;
            output[3] = *v3;

            /* Interpolate normals */
            interpolateVec3(v1->nxyz, v2->nxyz, t1, output[1].nxyz);
            interpolateVec3(v3->nxyz, v2->nxyz, t2, output[2].nxyz);

            /* Interpolate texcoords */
            interpolateVec2(v1->uv, v2->uv, t1, output[1].uv);
            interpolateVec2(v3->uv, v2->uv, t2, output[2].uv);

            interpolateColour((uint32_t*) &v1->argb, (uint32_t*) &v2->argb, t1, (uint32_t*) &output[1].argb);
            interpolateColour((uint32_t*) &v3->argb, (uint32_t*) &v2->argb, t2, (uint32_t*) &output[2].argb);

            output[0].flags = VERTEX_CMD;
            output[1].flags = VERTEX_CMD;
            output[2].flags = VERTEX_CMD;
            output[3].flags = VERTEX_CMD_EOL;

            aligned_vector_push_back(outBuffer, output, 4);
        }
    }
}
