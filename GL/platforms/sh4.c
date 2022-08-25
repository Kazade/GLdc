#include "../platform.h"
#include "sh4.h"


#define CLIP_DEBUG 0

#define PVR_VERTEX_BUF_SIZE 2560 * 256

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define SQ_BASE_ADDRESS 0xe0000000

static volatile uint32_t* PVR_LMMODE0 = (uint32_t*) 0xA05F6884;


GL_FORCE_INLINE bool glIsVertex(const float flags) {
    return flags == GPU_CMD_VERTEX_EOL || flags == GPU_CMD_VERTEX;
}

GL_FORCE_INLINE bool glIsLastVertex(const float flags) {
    return flags == GPU_CMD_VERTEX_EOL;
}

void InitGPU(_Bool autosort, _Bool fsaa) {
    pvr_init_params_t params = {
        /* Enable opaque and translucent polygons with size 32 and 32 */
        {PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32},
        PVR_VERTEX_BUF_SIZE, /* Vertex buffer size */
        0, /* No DMA */
        fsaa, /* No FSAA */
        (autosort) ? 0 : 1 /* Disable translucent auto-sorting to match traditional GL */
    };

    pvr_init(&params);
}

void SceneBegin() {
    pvr_wait_ready();
    pvr_scene_begin();

    QACR0 = 0x11;  /* Enable the direct texture path by setting the higher two bits */
    QACR1 = 0x11;
}

void SceneListBegin(GPUList list) {
    pvr_list_begin(list);
}

GL_FORCE_INLINE void _glPerspectiveDivideVertex(Vertex* vertex, const float h) {
    const float f = MATH_Fast_Invert(vertex->w);

    /* Convert to NDC and apply viewport */
    vertex->xyz[0] = __builtin_fmaf(
        VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
    );

    vertex->xyz[1] = h - __builtin_fmaf(
        VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
    );

    /* Orthographic projections need to use invZ otherwise we lose
    the depth information. As w == 1, and clip-space range is -w to +w
    we add 1.0 to the Z to bring it into range. We add a little extra to
    avoid a divide by zero.
    */
    if(unlikely(vertex->w == 1.0f)) {
        vertex->xyz[2] = MATH_Fast_Invert(1.0001f + vertex->xyz[2]);
    } else {
        vertex->xyz[2] = f;
    }
}

static uint32_t *d;  // SQ target

GL_FORCE_INLINE void _glSubmitHeaderOrVertex(const Vertex* v) {
#ifndef NDEBUG
    gl_assert(!isnan(v->xyz[2]));
    gl_assert(!isnan(v->w));
#endif

#if CLIP_DEBUG
    printf("Submitting: %x (%x)\n", v, v->flags);
#endif

    uint32_t *s = (uint32_t*) v;
    __asm__("pref @%0" : : "r"(s + 8));  /* prefetch 32 bytes for next loop */
    d[0] = *(s++);
    d[1] = *(s++);
    d[2] = *(s++);
    d[3] = *(s++);
    d[4] = *(s++);
    d[5] = *(s++);
    d[6] = *(s++);
    d[7] = *(s++);
    __asm__("pref @%0" : : "r"(d));
    d += 8;
}

static struct {
    Vertex* v;
    int visible;
} triangle[3];

static int tri_count = 0;
static int strip_count = 0;

GL_FORCE_INLINE void interpolateColour(const uint8_t* v1, const uint8_t* v2, const float t, uint8_t* out) {
    const int MASK1 = 0x00FF00FF;
    const int MASK2 = 0xFF00FF00;

    const int f2 = 256 * t;
    const int f1 = 256 - f2;

    const uint32_t a = *(uint32_t*) v1;
    const uint32_t b = *(uint32_t*) v2;

    *((uint32_t*) out) = (((((a & MASK1) * f1) + ((b & MASK1) * f2)) >> 8) & MASK1) |
            (((((a & MASK2) * f1) + ((b & MASK2) * f2)) >> 8) & MASK2);
}

GL_FORCE_INLINE void _glClipEdge(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    /* Clipping time! */
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];

    const float epsilon = (d0 < d1) ? -0.00001f : 0.00001f;

    float t = MATH_Fast_Divide(d0, (d0 - d1)) + epsilon;

    t = (t > 1.0f) ? 1.0f : t;
    t = (t < 0.0f) ? 0.0f : t;

    vout->xyz[0] = __builtin_fmaf(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = __builtin_fmaf(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = __builtin_fmaf(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);
    vout->w = __builtin_fmaf(v2->w - v1->w, t, v1->w);

    vout->uv[0] = __builtin_fmaf(v2->uv[0] - v1->uv[0], t, v1->uv[0]);
    vout->uv[1] = __builtin_fmaf(v2->uv[1] - v1->uv[1], t, v1->uv[1]);

    interpolateColour(v1->bgra, v2->bgra, t, vout->bgra);
}

GL_FORCE_INLINE void ClearTriangle() {
    tri_count = 0;
}

GL_FORCE_INLINE void ShiftTriangle() {
    if(!tri_count) {
        return;
    }

    tri_count--;
    triangle[0] = triangle[1];
    triangle[1] = triangle[2];

#ifndef NDEBUG
    triangle[2].v = NULL;
    triangle[2].visible = false;
#endif
}


GL_FORCE_INLINE void ShiftRotateTriangle() {
    if(!tri_count) {
        return;
    }

    if(triangle[0].v < triangle[1].v) {
        triangle[0] = triangle[2];
    } else {
        triangle[1] = triangle[2];
    }

    tri_count--;
}

#define SPAN_SORT_CFG 0x005F8030

void SceneListSubmit(void* src, int n) {
    /* Do everything, everywhere, all at once */
    PVR_SET(SPAN_SORT_CFG, 0x0);

    /* Prep store queues */
    d = (uint32_t*) SQ_BASE_ADDRESS;

    *PVR_LMMODE0 = 0x0; /* Enable 64bit mode */

    /* Perform perspective divide on each vertex */
    Vertex* vertex = (Vertex*) src;

    const float h = GetVideoMode()->height;

    if(!ZNEAR_CLIPPING_ENABLED) {
        for(int i = 0; i < n; ++i, ++vertex) {
            PREFETCH(vertex + 1);
            if(glIsVertex(vertex->flags)) {
                _glPerspectiveDivideVertex(vertex, h);
            }
            _glSubmitHeaderOrVertex(vertex);
        }

        /* Wait for both store queues to complete */
        d = (uint32_t *) SQ_BASE_ADDRESS;
        d[0] = d[8] = 0;

        return;
    }

    tri_count = 0;
    strip_count = 0;

#if CLIP_DEBUG
    printf("----\n");
#endif

    for(int i = 0; i < n; ++i, ++vertex) {
        PREFETCH(vertex + 1);

        bool is_last_in_strip = glIsLastVertex(vertex->flags);

        /* Wait until we fill the triangle */
        if(tri_count < 3) {
            if(likely(glIsVertex(vertex->flags))) {
                triangle[tri_count].v = vertex;
                triangle[tri_count].visible = vertex->xyz[2] >= -vertex->w;
                tri_count++;
                strip_count++;
            } else {
                /* We hit a header */
                tri_count = 0;
                strip_count = 0;
                _glSubmitHeaderOrVertex(vertex);
            }

            if(tri_count < 3) {
                continue;
            }
        }

#if CLIP_DEBUG
        printf("SC: %d\n", strip_count);
#endif

        /* If we got here, then triangle contains 3 vertices */
        int visible_mask = triangle[0].visible | (triangle[1].visible << 1) | (triangle[2].visible << 2);
        if(visible_mask == 7) {
#if CLIP_DEBUG
            printf("Visible\n");
#endif
            /* All the vertices are visible! We divide and submit v0, then shift */
            _glPerspectiveDivideVertex(vertex - 2, h);
            _glSubmitHeaderOrVertex(vertex - 2);

            if(is_last_in_strip) {
                _glPerspectiveDivideVertex(vertex - 1, h);
                _glSubmitHeaderOrVertex(vertex - 1);
                _glPerspectiveDivideVertex(vertex, h);
                _glSubmitHeaderOrVertex(vertex);
                tri_count = 0;
                strip_count = 0;
            }

            ShiftRotateTriangle();

        } else if(visible_mask) {
            /* Clipping time!

                There are 6 distinct possibilities when clipping a triangle. 3 of them result
                in another triangle, 3 of them result in a quadrilateral.

                Assuming you iterate the edges of the triangle in order, and create a new *visible*
                vertex when you cross the plane, and discard vertices behind the plane, then the only
                difference between the two cases is that the final two vertices that need submitting have
                to be reversed.

                Unfortunately we have to copy vertices here, because if we persp-divide a vertex it may
                be used in a subsequent triangle in the strip and would end up being double divided.
            */
#if CLIP_DEBUG
            printf("Clip: %d, SC: %d\n", visible_mask, strip_count);
            printf("%d, %d, %d\n", triangle[0].v - (Vertex*) src - 1, triangle[1].v - (Vertex*) src - 1, triangle[2].v - (Vertex*) src - 1);
#endif
            Vertex tmp;
            if(strip_count > 3) {
#if CLIP_DEBUG
                printf("Flush\n");
#endif
                tmp = *(vertex - 2);
                /* If we had triangles ahead of this one, submit and finalize */
                _glPerspectiveDivideVertex(&tmp, h);
                _glSubmitHeaderOrVertex(&tmp);

                tmp = *(vertex - 1);
                tmp.flags = GPU_CMD_VERTEX_EOL;
                _glPerspectiveDivideVertex(&tmp, h);
                _glSubmitHeaderOrVertex(&tmp);
            }

            switch(visible_mask) {
                case 1: {
                    /* 0, 0a, 2a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 2: {
                    /* 0a, 1, 1a */
                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 3: {
                    /* 0, 1, 2a, 1a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 4: {
                    /* 1a, 2, 2a */
                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 5: {
                    /* 0, 0a, 2, 1a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 6: {
                    /* 0a, 1, 2a, 2 */
                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                default:
                break;
            }

            /* If this was the last in the strip, we don't need to
            submit anything else, we just wipe the tri_count */
            if(is_last_in_strip) {
                tri_count = 0;
                strip_count = 0;
            } else {
                ShiftRotateTriangle();
                strip_count = 2;
            }
        } else {
            /* Invisible? Move to the next in the strip */

            if(is_last_in_strip) {
                tri_count = 0;
                strip_count = 0;
            }
            strip_count = 2;
            ShiftRotateTriangle();
        }
    }

    /* Wait for both store queues to complete */
    d = (uint32_t *)0xe0000000;
    d[0] = d[8] = 0;
}

void SceneListFinish() {
    pvr_list_finish();
}

void SceneFinish() {
    pvr_scene_finish();
}

const VideoMode* GetVideoMode() {
    static VideoMode mode;
    mode.width = vid_mode->width;
    mode.height = vid_mode->height;
    return &mode;
}
