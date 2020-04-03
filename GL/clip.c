#include <float.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _arch_dreamcast
#include <dc/pvr.h>
#else
#define PVR_PACK_COLOR(a, r, g, b) {}
#endif

#include "profiler.h"
#include "private.h"
#include "../containers/aligned_vector.h"

static unsigned char ZCLIP_ENABLED = 1;

unsigned char _glIsClippingEnabled() {
    return ZCLIP_ENABLED;
}

void _glEnableClipping(unsigned char v) {
    ZCLIP_ENABLED = v;
}

inline float _glClipLineToNearZ(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];

    /* We need to shift 't' a little, to avoid the possibility that a
     * rounding error leaves the new vertex behind the near plane. We shift
     * according to the direction we're clipping across the plane */
    const float epsilon = (d0 < d1) ? -0.000001 : 0.000001;

    float t = MATH_Fast_Divide(d0, (d0 - d1)) + epsilon;

    vout->xyz[0] = MATH_fmac(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = MATH_fmac(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = MATH_fmac(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);

    /*
    printf(
        "(%f, %f, %f, %f) -> %f -> (%f, %f, %f, %f) = (%f, %f, %f)\n",
        v1->xyz[0], v1->xyz[1], v1->xyz[2], v1->w, t,
        v2->xyz[0], v2->xyz[1], v2->xyz[2], v2->w,
        vout->xyz[0], vout->xyz[1], vout->xyz[2]
    );*/

    return t;
}

GL_FORCE_INLINE void interpolateFloat(const float v1, const float v2, const float t, float* out) {
    *out = MATH_fmac(v2 - v1,t, v1);
}

GL_FORCE_INLINE void interpolateVec2(const float* v1, const float* v2, const float t, float* out) {
    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
}

GL_FORCE_INLINE void interpolateVec3(const float* v1, const float* v2, const float t, float* out) {
    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
    interpolateFloat(v1[2], v2[2], t, &out[2]);
}

GL_FORCE_INLINE void interpolateVec4(const float* v1, const float* v2, const float t, float* out) {
    interpolateFloat(v1[0], v2[0], t, &out[0]);
    interpolateFloat(v1[1], v2[1], t, &out[1]);
    interpolateFloat(v1[2], v2[2], t, &out[2]);
    interpolateFloat(v1[3], v2[3], t, &out[3]);
}

GL_FORCE_INLINE void interpolateColour(const uint8_t* v1, const uint8_t* v2, const float t, uint8_t* out) {
    out[0] = v1[0] + (uint32_t) (((float) (v2[0] - v1[0])) * t);
    out[1] = v1[1] + (uint32_t) (((float) (v2[1] - v1[1])) * t);
    out[2] = v1[2] + (uint32_t) (((float) (v2[2] - v1[2])) * t);
    out[3] = v1[3] + (uint32_t) (((float) (v2[3] - v1[3])) * t);
}

const uint32_t VERTEX_CMD_EOL = 0xf0000000;
const uint32_t VERTEX_CMD = 0xe0000000;

typedef struct {
    Vertex vertex[3];
    VertexExtra extra[3];
    uint8_t visible;
} Triangle;

void _glClipTriangle(const Triangle* triangle, const uint8_t visible, SubmissionTarget* target, const uint8_t flatShade) {
    Vertex* last = NULL;
    VertexExtra* veLast = NULL;

    const Vertex* vertices = triangle->vertex;
    const VertexExtra* extras = triangle->extra;

    char* bgra = (char*) vertices[2].bgra;

    /* Used when flat shading is enabled */
    uint32_t finalColour = *((uint32_t*) bgra);

    Vertex tmp;
    VertexExtra veTmp;

    uint8_t pushedCount = 0;

#define IS_VISIBLE(x) (visible & (1 << (2 - (x)))) > 0

#define PUSH_VERT(vert, ve) \
    last = aligned_vector_push_back(&target->output->vector, vert, 1); \
    last->flags = VERTEX_CMD; \
    veLast = aligned_vector_push_back(target->extras, ve, 1); \
    ++pushedCount;

#define CLIP_TO_PLANE(vert1, ve1, vert2, ve2) \
    do { \
        float t = _glClipLineToNearZ((vert1), (vert2), &tmp); \
        interpolateFloat((vert1)->w, (vert2)->w, t, &tmp.w); \
        interpolateVec2((vert1)->uv, (vert2)->uv, t, tmp.uv); \
        interpolateVec3((ve1)->nxyz, (ve2)->nxyz, t, veTmp.nxyz); \
        interpolateVec2((ve1)->st, (ve2)->st, t, veTmp.st); \
        if(flatShade) { \
            interpolateColour((const uint8_t*) &finalColour, (const uint8_t*) &finalColour, t, tmp.bgra); \
        } else { interpolateColour((vert1)->bgra, (vert2)->bgra, t, tmp.bgra); } \
    } while(0); \

    uint8_t v0 = IS_VISIBLE(0);
    uint8_t v1 = IS_VISIBLE(1);
    uint8_t v2 = IS_VISIBLE(2);
    if(v0) {
        PUSH_VERT(&vertices[0], &extras[0]);
    }

    if(v0 != v1) {
        CLIP_TO_PLANE(&vertices[0], &extras[0], &vertices[1], &extras[1]);
        PUSH_VERT(&tmp, &veTmp);
    }

    if(v1) {
        PUSH_VERT(&vertices[1], &extras[1]);
    }

    if(v1 != v2) {
        CLIP_TO_PLANE(&vertices[1], &extras[1], &vertices[2], &extras[2]);
        PUSH_VERT(&tmp, &veTmp);
    }

    if(v2) {
        PUSH_VERT(&vertices[2], &extras[2]);
    }

    if(v2 != v0) {
        CLIP_TO_PLANE(&vertices[2], &extras[2], &vertices[0], &extras[0]);
        PUSH_VERT(&tmp, &veTmp);
    }

    if(pushedCount == 4) {
        Vertex* prev = last - 1;
        VertexExtra* prevVe = veLast - 1;

        tmp = *prev;
        veTmp = *prevVe;

        *prev = *last;
        *prevVe = *veLast;

        *last = tmp;
        *veLast = veTmp;

        prev->flags = VERTEX_CMD;
        last->flags = VERTEX_CMD_EOL;
    } else {
        /* Set the last flag to the end of the new strip */
        last->flags = VERTEX_CMD_EOL;
    }
}

static inline void markDead(Vertex* vert) {
    vert->flags = VERTEX_CMD_EOL;

    // If we're debugging, wipe out the xyz
#ifndef NDEBUG
    typedef union {
        float* f;
        int* i;
    } cast;

    cast v1, v2, v3;
    v1.f = &vert->xyz[0];
    v2.f = &vert->xyz[1];
    v3.f = &vert->xyz[2];

    *v1.i = 0xDEADBEEF;
    *v2.i = 0xDEADBEEF;
    *v3.i = 0xDEADBEEF;
#endif
}

#define B000 0
#define B111 7
#define B100 4
#define B010 2
#define B001 1
#define B101 5
#define B011 3
#define B110 6

#define MAX_CLIP_TRIANGLES 255

void _glClipTriangleStrip(SubmissionTarget* target, uint8_t fladeShade) {
    static Triangle TO_CLIP[MAX_CLIP_TRIANGLES];
    static uint8_t CLIP_COUNT = 0;

    CLIP_COUNT = 0;

    Vertex* vertex = _glSubmissionTargetStart(target);
    const Vertex* end = _glSubmissionTargetEnd(target);
    const Vertex* start = vertex;

    int32_t triangle = -1;

    /* Go to the (potential) end of the first triangle */
    vertex++;

    uint32_t vi1, vi2, vi3;

    while(vertex < end) {
        vertex++;
        triangle++;

        uint8_t even = (triangle % 2) == 0;
        Vertex* v1 = (even) ? vertex - 2 : vertex - 1;
        Vertex* v2 = (even) ? vertex - 1 : vertex - 2;
        Vertex* v3 = vertex;

        /* Skip ahead if we don't have a complete triangle yet */
        if(v1->flags != VERTEX_CMD || v2->flags != VERTEX_CMD) {
            triangle = -1;
            continue;
        }

        /* Indexes into extras array */
        vi1 = v1 - start;
        vi2 = v2 - start;
        vi3 = v3 - start;

        /*
         * A vertex is visible if it's in front of the camera (W > 0)
         * and it's in front of the near plane (Z > -W)
         */
        uint8_t visible = (
            ((v1->w >= 0 && v1->xyz[2] >= -v1->w) ? 4 : 0) |
            ((v2->w >= 0 && v2->xyz[2] >= -v2->w) ? 2 : 0) |
            ((v3->w >= 0 && v3->xyz[2] >= -v3->w) ? 1 : 0)
        );

        switch(visible) {
            case B111:
                /* All visible? Do nothing */
                continue;
            break;
            case B000:
                /*
                    It is not possible that this is any trangle except the first
                    in a strip. That's because:
                    - It's either the first triangle submitted
                    - A previous triangle must have been clipped and the strip
                      restarted behind the plane

                    So, we effectively reboot the strip. We mark the first vertex
                    as the end (so it's ignored) then mark the next two as the
                    start of a new strip. Then if the next triangle crosses
                    back into view, we clip correctly. This will potentially
                    result in a bunch of pointlessly submitted vertices.

                    FIXME: Skip submitting those verts
                */

                /* Even though this is always the first in the strip, it can also
                 * be the last */
                if(v3->flags == VERTEX_CMD_EOL) {
                    /* Wipe out the triangle */
                    markDead(v1);
                    markDead(v2);
                    markDead(v3);
                } else {
                    markDead(v1);
                    swapVertex(v2, v3);
                    triangle = -1;
                    v2->flags = VERTEX_CMD;
                    v3->flags = VERTEX_CMD;
                }
            break;
            case B100:
            case B010:
            case B001:
            case B101:
            case B011:
            case B110:
                assert(CLIP_COUNT < MAX_CLIP_TRIANGLES);

                /* Store the triangle for clipping */
                TO_CLIP[CLIP_COUNT].vertex[0] = *v1;
                TO_CLIP[CLIP_COUNT].vertex[1] = *v2;
                TO_CLIP[CLIP_COUNT].vertex[2] = *v3;

                VertexExtra* ve1 = (VertexExtra*) aligned_vector_at(target->extras, vi1);
                VertexExtra* ve2 = (VertexExtra*) aligned_vector_at(target->extras, vi2);
                VertexExtra* ve3 = (VertexExtra*) aligned_vector_at(target->extras, vi3);

                TO_CLIP[CLIP_COUNT].extra[0] = *ve1;
                TO_CLIP[CLIP_COUNT].extra[1] = *ve2;
                TO_CLIP[CLIP_COUNT].extra[2] = *ve3;

                TO_CLIP[CLIP_COUNT].visible = visible;
                ++CLIP_COUNT;

                /*
                    OK so here's the clever bit. If any triangle except
                    the first or last needs clipping, then the next one does aswell
                    (you can't draw a plane through a single triangle in the middle of a
                    strip, only 2+). This means we can clip in pairs which frees up two
                    vertices in the middle of the strip, which is exactly the space
                    we need to restart the triangle strip after the next triangle
                */
                if(v3->flags == VERTEX_CMD_EOL) {
                    /* Last triangle in strip so end a vertex early */
                    if(triangle == 0) {
                        // Wipe out the triangle completely
                        markDead(v1);
                        markDead(v2);
                    } else {
                        // End the strip
                        (vertex - 1)->flags = VERTEX_CMD_EOL;
                    }

                    markDead(vertex);

                    triangle = -1;
                } else if(triangle == 0) {
                    /* First triangle in strip, remove first vertex */
                    markDead(v1);

                    v2->flags = VERTEX_CMD;
                    v3->flags = VERTEX_CMD;

                    triangle = -1;
                } else {                    
                    Vertex* v4 = v3 + 1;
                    uint32_t vi4 = v4 - start;

                    TO_CLIP[CLIP_COUNT].vertex[0] = *v3;
                    TO_CLIP[CLIP_COUNT].vertex[1] = *v2;
                    TO_CLIP[CLIP_COUNT].vertex[2] = *v4;

                    VertexExtra* ve4 = (VertexExtra*) aligned_vector_at(target->extras, vi4);
                    TO_CLIP[CLIP_COUNT].extra[0] = *(VertexExtra*) aligned_vector_at(target->extras, vi3);
                    TO_CLIP[CLIP_COUNT].extra[1] = *(VertexExtra*) aligned_vector_at(target->extras, vi2);
                    TO_CLIP[CLIP_COUNT].extra[2] = *ve4;

                    visible = ((v3->w > 0) ? 4 : 0) | ((v2->w > 0) ? 2 : 0) | ((v4->w > 0) ? 1 : 0);

                    TO_CLIP[CLIP_COUNT].visible = visible;
                    ++CLIP_COUNT;

                    // Restart strip
                    triangle = -1;

                    // Mark the second vertex as the end of the strip
                    (vertex - 1)->flags = VERTEX_CMD_EOL;

                    if(v4->flags == VERTEX_CMD_EOL) {
                        markDead(v3);
                        markDead(v4);
                    } else {
                        // Swap the next vertices to start a new strip
                        swapVertex(v3, v4);
                        v3->flags = VERTEX_CMD;
                        v4->flags = VERTEX_CMD;

                        /* Swap the extra data too */
                        VertexExtra t = *ve4;
                        *ve3 = *ve4;
                        *ve4 = t;
                    }
                }
            break;
            default:
                break;
        }
    }

    /* Now, clip all the triangles and append them to the output */
    GLushort i;
    for(i = 0; i < CLIP_COUNT; ++i) {
        _glClipTriangle(&TO_CLIP[i], TO_CLIP[i].visible, target, fladeShade);
    }
}
