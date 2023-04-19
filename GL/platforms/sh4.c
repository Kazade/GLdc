#include "../platform.h"
#include "sh4.h"


#define CLIP_DEBUG 0

#define PVR_VERTEX_BUF_SIZE 2560 * 256

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define SQ_BASE_ADDRESS (uint32_t *)(void *) \
    (0xe0000000 | (((uint32_t)0x10000000) & 0x03ffffe0))


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
}

void SceneListBegin(GPUList list) {
    pvr_list_begin(list);
}

GL_FORCE_INLINE float _glFastInvert(float x) {
    return (1.f / __builtin_sqrtf(x * x));
}

GL_FORCE_INLINE void _glPerspectiveDivideVertex(Vertex* vertex, const float h) {
    const float f = _glFastInvert(vertex->w);

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

    vertex->xyz[2] = (vertex->w == 1.0f) ? _glFastInvert(1.0001f + vertex->xyz[2]) : f;
}

GL_FORCE_INLINE void _glSubmitHeaderOrVertex(volatile uint32_t* d, const Vertex* v) {
#ifndef NDEBUG
    gl_assert(!isnan(v->xyz[2]));
    gl_assert(!isnan(v->w));
#endif

#if CLIP_DEBUG
    printf("Submitting: %x (%x)\n", v, v->flags);
#endif

    uint32_t *s = (uint32_t*) v;
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


static inline void interpolateColour(const uint32_t* a, const uint32_t* b, const float t, uint32_t* out) {
    const static uint32_t MASK1 = 0x00FF00FF;
    const static uint32_t MASK2 = 0xFF00FF00;

    const uint32_t f2 = 256 * t;
    const uint32_t f1 = 256 - f2;

    *out = (((((*a & MASK1) * f1) + ((*b & MASK1) * f2)) >> 8) & MASK1) |
            (((((*a & MASK2) * f1) + ((*b & MASK2) * f2)) >> 8) & MASK2);
}

static inline void _glClipEdge(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    /* Clipping time! */
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];
    const float sign = ((2.0f * (d1 < d0)) - 1.0f);
    const float epsilon = -0.00001f * sign;
    const float n = (d0 - d1);
    const float r = (1.f / sqrtf(n * n)) * sign;
    float t = fmaf(r, d0, epsilon);

    vout->xyz[0] = fmaf(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = fmaf(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = fmaf(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);
    vout->w = fmaf(v2->w - v1->w, t, v1->w);

    vout->uv[0] = fmaf(v2->uv[0] - v1->uv[0], t, v1->uv[0]);
    vout->uv[1] = fmaf(v2->uv[1] - v1->uv[1], t, v1->uv[1]);

    interpolateColour((uint32_t*) v1->bgra, (uint32_t*) v2->bgra, t, (uint32_t*) vout->bgra);
}

#define SPAN_SORT_CFG 0x005F8030
static volatile int *pvrdmacfg = (int*)0xA05F6888;
static volatile int *qacr = (int*)0xFF000038;

void SceneListSubmit(void* src, int n) {
    /* You need at least a header, and 3 vertices to render anything */
    if(n < 4) {
        return;
    }

    const float h = GetVideoMode()->height;

    PVR_SET(SPAN_SORT_CFG, 0x0);

    //Set PVR DMA registers
    pvrdmacfg[0] = 1;
    pvrdmacfg[1] = 0;

    //Set QACR registers
    qacr[1] = qacr[0] = 0x11;

    volatile uint32_t *d = SQ_BASE_ADDRESS;

    int8_t queue_head = 0;
    int8_t queue_tail = 0;

    Vertex __attribute__((aligned(32))) queue[3];
    const int queue_capacity = sizeof(queue) / sizeof(Vertex);

    Vertex* vertex = (Vertex*) src;
    uint32_t visible_mask = 0;

    for(int i = 0; i < n; ++i) {
        Vertex* v = vertex + i;
        fprintf(stderr, "{%f, %f, %f, %f},\n", v->xyz[0], v->xyz[1], v->xyz[2], v->w);
    }

    /* Assume first entry is a header */
    _glSubmitHeaderOrVertex(d, vertex++);

    /* Push first 2 vertices of the strip */
    memcpy_vertex(&queue[0], vertex++);
    memcpy_vertex(&queue[1], vertex++);
    visible_mask = ((queue[0].xyz[2] >= -queue[0].w) << 1) | ((queue[1].xyz[2] >= -queue[1].w) << 2);
    queue_tail = 2;
    n -= 3;

    while(n--) {
        Vertex* self = &queue[queue_tail];
        memcpy_vertex(self, vertex++);
        visible_mask = (visible_mask >> 1) | ((self->xyz[2] >= -self->w) << 2);  // Push new vertex
        queue_tail = (queue_tail + 1) % queue_capacity;

        switch(visible_mask) {
            case 0:
                queue_head = (queue_head + 1) % queue_capacity;
                continue;
            break;
            case 7:
                /* All visible, push the first vertex and move on */
                _glPerspectiveDivideVertex(&queue[queue_head], h);
                _glSubmitHeaderOrVertex(d, &queue[queue_head]);
                queue_head = (queue_head + 1) % queue_capacity;

                if(glIsLastVertex(self->flags)) {
                    /* If this was the last vertex in the strip, we clear the
                     * triangle out */
                    while(queue_head != queue_tail) {
                        _glPerspectiveDivideVertex(&queue[queue_head], h);
                        _glSubmitHeaderOrVertex(d, &queue[queue_head]);
                        queue_head = (queue_head + 1) % queue_capacity;
                    }

                    visible_mask = 0;
                }
            break;
            case 1:
                /* First vertex was visible */
                {
                        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v2, v0, &b);
                        a.flags = GPU_CMD_VERTEX;

                        /* If v2 was the last in the strip, then b should be. If it wasn't
                        we'll create a degenerate triangle by adding b twice in a row so that the
                        strip processing will continue correctly after crossing the plane so it can
                        cross back*/
                        b.flags = v2->flags;

                        _glPerspectiveDivideVertex(v0, h);
                        _glSubmitHeaderOrVertex(d, v0);
                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);
                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &b);

                        /* But skip the vertices that are already there */
                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            case 2:
                /* Second vertex was visible. In self case we need to create a triangle and produce
                two new vertices: 1-2, and 2-3. */
                {
                        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v1, v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX_EOL;

                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);

                        _glPerspectiveDivideVertex(v1, h);
                        _glSubmitHeaderOrVertex(d, v1);

                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);

                        /* But skip the vertices that are already there */
                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            case 3:  /* First and second vertex were visible */
                    {
                        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v1, v2, &a);
                        _glClipEdge(v2, v0, &b);

                        a.flags = v2->flags;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(v0, h);
                        _glSubmitHeaderOrVertex(d, v0);

                        _glPerspectiveDivideVertex(v1, h);
                        _glSubmitHeaderOrVertex(d, v1);

                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);

                        _glSubmitHeaderOrVertex(d, v1);

                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);

                        /* But skip the vertices that are already there */
                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            case 4:
                /* Third vertex was visible. */
                {
                        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex v2 = queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(&v2, v0, &a);
                        _glClipEdge(v1, &v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);

                        _glSubmitHeaderOrVertex(d, &a);

                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);

                        _glPerspectiveDivideVertex(&v2, h);
                        _glSubmitHeaderOrVertex(d, &v2);

                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            case 5:  /* First and third vertex were visible */
                {
                        Vertex __attribute__((aligned(32))) a, b, c;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v1, v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(v0, h);
                        _glSubmitHeaderOrVertex(d, v0);

                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);

                        uint32_t v2_flags = v2->flags;
                        v2->flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(v2, h);
                        _glSubmitHeaderOrVertex(d, v2);

                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);

                        v2->flags = v2_flags;
                        _glSubmitHeaderOrVertex(d, v2);

                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            case 6:  /* Second and third vertex were visible */
                {
                        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v2, v0, &b);

                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(&a, h);
                        _glSubmitHeaderOrVertex(d, &a);

                        _glPerspectiveDivideVertex(v1, h);
                        _glSubmitHeaderOrVertex(d, v1);

                        _glPerspectiveDivideVertex(&b, h);
                        _glSubmitHeaderOrVertex(d, &b);

                        _glSubmitHeaderOrVertex(d, v1);

                        _glPerspectiveDivideVertex(v2, h);
                        _glSubmitHeaderOrVertex(d, v2);

                        queue_head = (queue_head + 3) % queue_capacity;
                        visible_mask = 0;
                }
            break;
            default:
                break;
        }

        /* Submit the beginning of the next strip (2 verts, maybe a header) */
        int8_t v = 0;
        while(v < 2 && n > 1) {
            if(!glIsVertex(vertex->flags)) {
                _glSubmitHeaderOrVertex(d, vertex);
            } else {
                memcpy_vertex(&queue[queue_tail], vertex++);
                visible_mask = (visible_mask >> 1) | ((queue[queue_tail].xyz[2] >= -queue[queue_tail].w) << 2);  // Push new vertex
                queue_tail = (queue_tail + 1) % queue_capacity;
                ++v;
            }
            --n;
        }

    }
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
