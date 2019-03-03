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

unsigned char _glIsClippingEnabled() {
    return ZCLIP_ENABLED;
}

void _glEnableClipping(unsigned char v) {
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

static inline void interpolateColour(const uint8_t* v1, const uint8_t* v2, const float t, uint8_t* out) {
    out[0] = v1[0] + (uint32_t) (((float) (v2[0] - v1[0])) * t);
    out[1] = v1[1] + (uint32_t) (((float) (v2[1] - v1[1])) * t);
    out[2] = v1[2] + (uint32_t) (((float) (v2[2] - v1[2])) * t);
    out[3] = v1[3] + (uint32_t) (((float) (v2[3] - v1[3])) * t);
}

const uint32_t VERTEX_CMD_EOL = 0xf0000000;
const uint32_t VERTEX_CMD = 0xe0000000;

void clipTriangle(const ClipVertex* vertices, const uint8_t visible, AlignedVector* output, const uint8_t flatShade) __attribute__((optimize("fast-math")));
void clipTriangle(const ClipVertex* vertices, const uint8_t visible, AlignedVector* output, const uint8_t flatShade) {
    uint8_t i, c = 0;


    uint8_t lastVisible = 255;
    ClipVertex* last = NULL;

    /* Used when flat shading is enabled */
    uint32_t finalColour = *((uint32_t*) vertices[2].bgra);

    for(i = 0; i < 4; ++i) {
        uint8_t thisIndex = (i == 3) ? 0 : i;

        ClipVertex next;
        next.flags = VERTEX_CMD;

        uint8_t thisVisible = (visible & (1 << (2 - thisIndex))) > 0;
        if(i > 0) {
            uint8_t lastIndex = (i == 3) ? 2 : thisIndex - 1;

            if(lastVisible < 255 && lastVisible != thisVisible) {
                const ClipVertex* v1 = &vertices[lastIndex];
                const ClipVertex* v2 = &vertices[thisIndex];
                float t;

                clipLineToNearZ(v1, v2, &next, &t);
                interpolateFloat(v1->w, v2->w, t, &next.w);
                interpolateVec3(v1->nxyz, v2->nxyz, t, next.nxyz);
                interpolateVec2(v1->uv, v2->uv, t, next.uv);
                interpolateVec2(v1->st, v2->st, t, next.st);

                if(flatShade) {
                    *((uint32_t*) next.bgra) = finalColour;
                } else {
                    interpolateColour(v1->bgra, v2->bgra, t, next.bgra);
                }

                last = aligned_vector_push_back(output, &next, 1);
                last->flags = VERTEX_CMD;
                ++c;
            }
        }

        if(thisVisible && i != 3) {
            last = aligned_vector_push_back(output, &vertices[thisIndex], 1);
            last->flags = VERTEX_CMD;
            ++c;
        }

        lastVisible = thisVisible;
    }

    if(last) {
        if(c == 4) {
            /* Convert to two triangles */
            ClipVertex newVerts[3];
            newVerts[0] = *(last - 3);
            newVerts[1] = *(last - 1);
            newVerts[2] = *(last);

            (last - 1)->flags = VERTEX_CMD_EOL;
            newVerts[0].flags = VERTEX_CMD;
            newVerts[1].flags = VERTEX_CMD;
            newVerts[2].flags = VERTEX_CMD_EOL;

            aligned_vector_resize(output, output->size - 1);
            aligned_vector_push_back(output, newVerts, 3);
        } else {
            last->flags = VERTEX_CMD_EOL;
        }

    }
}

static inline void markDead(ClipVertex* vert) {
    vert->flags = VERTEX_CMD_EOL;
}

void clipTriangleStrip2(AlignedVector* vertices, uint32_t offset, uint8_t fladeShade) {
    /* Room for clipping 16 triangles */
    typedef struct {
        ClipVertex vertex[3];
        uint8_t visible;
    } Triangle;

    static Triangle TO_CLIP[256];
    static uint8_t CLIP_COUNT = 0;

    CLIP_COUNT = 0;

    uint32_t i = 0;
    /* Skip the header */
    ClipVertex* header = (ClipVertex*) aligned_vector_at(vertices, offset);
    ClipVertex* vertex = header + 1;

    uint32_t count = vertices->size - offset;

    int32_t triangle = 0;

    /* Start at 3 due to the header */
    for(i = 3; i < count; ++i, ++triangle) {
        vertex = aligned_vector_at(vertices, offset + i);

        uint8_t even = (triangle % 2) == 0;
        ClipVertex* v1 = (even) ? vertex - 2 : vertex - 1;
        ClipVertex* v2 = (even) ? vertex - 1 : vertex - 2;
        ClipVertex* v3 = vertex;

        /* Skip ahead if we don't have a complete triangle yet */
        if(v1->flags != VERTEX_CMD || v2->flags != VERTEX_CMD) {
            triangle = -1;
            continue;
        }

        uint8_t visible = ((v1->w > 0) ? 4 : 0) | ((v2->w > 0) ? 2 : 0) | ((v3->w > 0) ? 1 : 0);

        switch(visible) {
            case 0b111:
                /* All visible? Do nothing */
                continue;
            break;
            case 0b000:
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
                    ClipVertex tmp = *v2;
                    *v2 = *v3;
                    *v3 = tmp;

                    triangle = -1;
                    v2->flags = VERTEX_CMD;
                    v3->flags = VERTEX_CMD;
                }
            break;
            case 0b100:
            case 0b010:
            case 0b001:
            case 0b101:
            case 0b011:
            case 0b110:
                /* Store the triangle for clipping */
                TO_CLIP[CLIP_COUNT].vertex[0] = *v1;
                TO_CLIP[CLIP_COUNT].vertex[1] = *v2;
                TO_CLIP[CLIP_COUNT].vertex[2] = *v3;
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
                    ClipVertex* v4 = vertex + 1;

                    TO_CLIP[CLIP_COUNT].vertex[0] = *v3;
                    TO_CLIP[CLIP_COUNT].vertex[1] = *v2;
                    TO_CLIP[CLIP_COUNT].vertex[2] = *v4;

                    visible = ((v3->w > 0) ? 4 : 0) | ((v2->w > 0) ? 2 : 0) | ((v4->w > 0) ? 1 : 0);

                    TO_CLIP[CLIP_COUNT].visible = visible;
                    ++CLIP_COUNT;

                    /* Restart strip */
                    triangle = -1;

                    /* Mark the second vertex as the end of the strip */
                    (vertex - 1)->flags = VERTEX_CMD_EOL;

                    if(v4->flags == VERTEX_CMD_EOL) {
                        markDead(vertex);
                        markDead(v4);
                    } else {
                        /* Swap the next vertices to start a new strip */
                        ClipVertex tmp = *vertex;
                        *vertex = *v4;
                        *v4 = tmp;

                        vertex->flags = VERTEX_CMD;
                        v4->flags = VERTEX_CMD;
                    }

                    i += 1;
                }
            break;
            default:
                break;
        }
    }

    /* Now, clip all the triangles and append them to the output */
    for(i = 0; i < CLIP_COUNT; ++i) {
        clipTriangle(TO_CLIP[i].vertex, TO_CLIP[i].visible, vertices, fladeShade);
    }
}
