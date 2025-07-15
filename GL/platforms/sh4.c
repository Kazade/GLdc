#include <float.h>

#include <dc/sq.h>

#include "../platform.h"
#include "sh4.h"


#define CLIP_DEBUG 0

#define PVR_VERTEX_BUF_SIZE 2560 * 256
#define PVR_OPB_COUNT       2

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


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
        (autosort) ? 0 : 1, /* Disable translucent auto-sorting to match traditional GL */
        PVR_OPB_COUNT /* Number of tile object pointer overflow bins. */
    };

    pvr_init(&params);

#ifndef _arch_sub_naomi
    /* If we're PAL and we're NOT VGA, then use 50hz by default. This is the safest
    thing to do. If someone wants to force 60hz then they can call vid_set_mode later and hopefully
    that'll work... */

    int cable = vid_check_cable();

    if(cable != CT_VGA) {
        int region = flashrom_get_region();
        if (region == FLASHROM_REGION_EUROPE) {
            printf("PAL region without VGA - enabling 50hz");
            vid_set_mode(DM_640x480_PAL_IL, PM_RGB565);
        }
    }
#endif
}

GL_FORCE_INLINE float _glFastInvert(float x) {
    return (1.0f / __builtin_sqrtf(x * x));
}

GL_FORCE_INLINE void _glPerspectiveDivideVertex(Vertex* vertex, int count) {
    TRACE();

    for(int v = 0; v < count; ++v) { 
        const float f = _glFastInvert(vertex[v].w);

        /* Convert to screenspace */
        /* (note that vertices have already been viewport transformed) */
        vertex[v].xyz[0] *= f;
        vertex[v].xyz[1] *= f;

        /* Orthographic projections need to use invZ otherwise we lose
        the depth information. As w == 1, and clip-space range is -w to +w
        we add 1.0 to the Z to bring it into range. We add a little extra to
        avoid a divide by zero.
        */
        if(vertex[v].w == 1.0f) {
            vertex[v].xyz[2] = _glFastInvert(1.0001f + vertex[v].xyz[2]);
        } else {
            vertex[v].xyz[2] = f;
        }
    }
}

static uintptr_t sq_dest_addr = 0;

static inline void _glPushHeaderOrVertex(Vertex* v, size_t count)  {
    TRACE();

#if CLIP_DEBUG
    fprintf(stderr, "{%f, %f, %f, %f}, // %x (%x)\n", v->xyz[0], v->xyz[1], v->xyz[2], v->w, v->flags, v);
#endif

    sq_fast_cpy((void *)sq_dest_addr, v, count);
}

static inline void _glClipEdge(const Vertex* const v1, const Vertex* const v2, Vertex* vout) {
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];
    const float t = (fabsf(d0) * (1.0f / sqrtf((d1 - d0) * (d1 - d0))));
    const float invt = 1.0f - t;

    vout->xyz[0] = invt * v1->xyz[0] + t * v2->xyz[0];
    vout->xyz[1] = invt * v1->xyz[1] + t * v2->xyz[1];
    vout->xyz[2] = invt * v1->xyz[2] + t * v2->xyz[2];
    vout->xyz[2] = (vout->xyz[2] < FLT_EPSILON) ? FLT_EPSILON : vout->xyz[2];

    vout->uv[0] = invt * v1->uv[0] + t * v2->uv[0];
    vout->uv[1] = invt * v1->uv[1] + t * v2->uv[1];

    vout->w = invt * v1->w + t * v2->w;

    vout->bgra[0] = invt * v1->bgra[0] + t * v2->bgra[0];
    vout->bgra[1] = invt * v1->bgra[1] + t * v2->bgra[1];
    vout->bgra[2] = invt * v1->bgra[2] + t * v2->bgra[2];
    vout->bgra[3] = invt * v1->bgra[3] + t * v2->bgra[3];
}

#define SPAN_SORT_CFG 0x005F8030
static volatile uint32_t* PVR_LMMODE0 = (uint32_t*) 0xA05F6884;
static volatile uint32_t *PVR_LMMODE1 = (uint32_t*) 0xA05F6888;

enum Visible {
    NONE_VISIBLE = 0,
    FIRST_VISIBLE = 1,
    SECOND_VISIBLE = 2,
    THIRD_VISIBLE = 4,
    FIRST_AND_SECOND_VISIBLE = FIRST_VISIBLE | SECOND_VISIBLE,
    SECOND_AND_THIRD_VISIBLE = SECOND_VISIBLE | THIRD_VISIBLE,
    FIRST_AND_THIRD_VISIBLE = FIRST_VISIBLE | THIRD_VISIBLE,
    ALL_VISIBLE = 7
};

static inline bool is_header(Vertex* v) {
    return !(v->flags == GPU_CMD_VERTEX || v->flags == GPU_CMD_VERTEX_EOL);
}

void SceneListSubmit(Vertex* vertices, int n) {
    TRACE();

    /* You need at least a header, and 3 vertices to render anything */
    if(n < 4) {
        return;
    }

    PVR_SET(SPAN_SORT_CFG, 0x0);

    //Set PVR DMA registers
    *PVR_LMMODE0 = 0;
    *PVR_LMMODE1 = 0;

#if CLIP_DEBUG
    fprintf(stderr, "----\n");

    Vertex* vertex = (Vertex*) vertices;
    for(int i = 0; i < n; ++i) {
        fprintf(stderr, "IN: {%f, %f, %f, %f}, // %x (%x)\n", vertex[i].xyz[0], vertex[i].xyz[1], vertex[i].xyz[2], vertex[i].w, vertex[i].flags, &vertex[i]);
    }
#endif

    /* This is a bit cumbersome - in some cases (particularly case 2)
       we finish the vertex submission with a duplicated final vertex so
       that the tri-strip can be continued. However, if the next triangle in the
       strip is not visible then the duplicated vertex would've been sent without
       the EOL flag. We won't know if we need the EOL flag or not when processing
       case 2. To workaround this we may queue a vertex temporarily here, in the normal
       case it will be submitted by the next iteration with the same flags it had, but
       in the invisible case it will be overridden to submit with EOL */
    static Vertex __attribute__((aligned(32))) qv;
    Vertex* queued_vertex = NULL;

#define QUEUE_VERTEX(v) \
    do { queued_vertex = &qv; *queued_vertex = *(v); } while(0)

#define SUBMIT_QUEUED_VERTEX(sflags) \
    do { if(queued_vertex) { queued_vertex->flags = (sflags); _glPushHeaderOrVertex(queued_vertex, 1); queued_vertex = NULL; } } while(0)

    int visible_mask = 0;
    sq_dest_addr = (uintptr_t)SQ_MASK_DEST(PVR_TA_INPUT);

    Vertex* v0 = vertices;
    for(int i = 0; i < n - 1; ++i, ++v0) {
        if(is_header(v0)) {
            _glPushHeaderOrVertex(v0, 1);
            visible_mask = 0;
            continue;
        }

        Vertex* v1 = v0 + 1;
        Vertex* v2 = (i < n - 2) ? v0 + 2 : NULL;

        assert(!is_header(v1));

        // We are trailing if we're on the penultimate vertex, or the next but one vertex is
        // an EOL, or v1 is an EOL (FIXME: possibly unnecessary and coverted by the other case?)
        bool is_trailing = (v1->flags == GPU_CMD_VERTEX_EOL) || ((v2) ? is_header(v2) : true);

        if(is_trailing) {
            // OK so we've hit a new context header
            // we need to finalize this strip and move on

            // If the last triangle was all visible, we need
            // to submit the last two vertices, any clipped triangles
            // would've
            if(visible_mask == ALL_VISIBLE) {
                SUBMIT_QUEUED_VERTEX(qv.flags);

                _glPerspectiveDivideVertex(v0, 2);
                v1->flags = GPU_CMD_VERTEX_EOL;
                _glPushHeaderOrVertex(v0, 2);
            } else {
                // If the previous triangle wasn't all visible, and we
                // queued a vertex - we force it to be EOL and submit
                SUBMIT_QUEUED_VERTEX(GPU_CMD_VERTEX_EOL);
            }

            i++;
            v0++;
            visible_mask = 0;
            continue;
        }

        visible_mask = (
            (v0->xyz[2] >= -v0->w) << 0 |
            (v1->xyz[2] >= -v1->w) << 1 |
            (v2->xyz[2] >= -v2->w) << 2
        );

        /* If we've gone behind the plane, we finish the strip
        otherwise we submit however it was */
        if(visible_mask == NONE_VISIBLE) {
            SUBMIT_QUEUED_VERTEX(GPU_CMD_VERTEX_EOL);
        } else {
            SUBMIT_QUEUED_VERTEX(qv.flags);
        }

#if CLIP_DEBUG
        fprintf(stderr, "0x%x 0x%x 0x%x -> %d\n", v0, v1, v2, visible_mask);
#endif

        Vertex __attribute__((aligned(32))) scratch[4];
        Vertex* a = &scratch[0], *b = &scratch[1], *c = &scratch[2], *d = &scratch[3];

        switch(visible_mask) {
            case ALL_VISIBLE:
                _glPerspectiveDivideVertex(v0, 1);
                QUEUE_VERTEX(v0);
            break;
            case NONE_VISIBLE:
                break;
            break;
            case FIRST_VISIBLE:
                _glClipEdge(v0, v1, a);
                a->flags = GPU_CMD_VERTEX;

                _glClipEdge(v2, v0, b);
                b->flags = GPU_CMD_VERTEX;

                _glPerspectiveDivideVertex(v0, 1);
                _glPushHeaderOrVertex(v0, 1);

                _glPerspectiveDivideVertex(a, 2);
                _glPushHeaderOrVertex(a, 2);

                QUEUE_VERTEX(b);
            break;
            case SECOND_VISIBLE:
                memcpy_vertex(c, v1);

                _glClipEdge(v0, v1, a);
                a->flags = GPU_CMD_VERTEX;

                _glClipEdge(v1, v2, b);
                b->flags = v2->flags;

                _glPerspectiveDivideVertex(a, 3);
                _glPushHeaderOrVertex(a, 1);

                _glPushHeaderOrVertex(c, 1);

                QUEUE_VERTEX(b);
            break;
            case THIRD_VISIBLE:
                memcpy_vertex(c, v2);

                _glClipEdge(v1, v2, a);
                a->flags = GPU_CMD_VERTEX;

                _glClipEdge(v2, v0, b);
                b->flags = GPU_CMD_VERTEX;

                _glPerspectiveDivideVertex(a, 3);
                _glPushHeaderOrVertex(a, 2);

                QUEUE_VERTEX(c);
            break;
            case FIRST_AND_SECOND_VISIBLE:
                memcpy_vertex(c, v1);

                _glClipEdge(v2, v0, b);
                b->flags = GPU_CMD_VERTEX;

                _glPerspectiveDivideVertex(v0, 1);
                _glPushHeaderOrVertex(v0, 1);

                _glClipEdge(v1, v2, a);
                a->flags = v2->flags;

                _glPerspectiveDivideVertex(a, 3);

                _glPushHeaderOrVertex(c, 1);

                _glPushHeaderOrVertex(b, 2);

                QUEUE_VERTEX(a);
            break;
            case SECOND_AND_THIRD_VISIBLE:
                memcpy_vertex(c, v1);
                memcpy_vertex(d, v2);

                _glClipEdge(v0, v1, a);
                a->flags = GPU_CMD_VERTEX;

                _glClipEdge(v2, v0, b);
                b->flags = GPU_CMD_VERTEX;

                _glPerspectiveDivideVertex(a, 4);
                _glPushHeaderOrVertex(a, 1);

                _glPushHeaderOrVertex(c, 1);

                _glPushHeaderOrVertex(b, 2);

                QUEUE_VERTEX(d);
            break;
            case FIRST_AND_THIRD_VISIBLE:
                memcpy_vertex(c, v2);
                c->flags = GPU_CMD_VERTEX;

                _glClipEdge(v0, v1, a);
                a->flags = GPU_CMD_VERTEX;

                _glClipEdge(v1, v2, b);
                b->flags = GPU_CMD_VERTEX;

                _glPerspectiveDivideVertex(v0, 1);
                _glPushHeaderOrVertex(v0, 1);

                _glPerspectiveDivideVertex(a, 3);
                _glPushHeaderOrVertex(a, 1);

                _glPushHeaderOrVertex(c, 1);

                _glPushHeaderOrVertex(b, 1);
                
                QUEUE_VERTEX(c);
            break;
            default:
                fprintf(stderr, "ERROR\n");
        }
    }

    SUBMIT_QUEUED_VERTEX(GPU_CMD_VERTEX_EOL);

    sq_wait();
}

void SceneBegin() {
    pvr_wait_ready();
    pvr_scene_begin();
}

static pvr_dr_state_t dr_state;
void SceneListBegin(GPUList list) {
    pvr_list_begin(list);
    /* Direct rendering auto acquires/releases store queue */
    pvr_dr_init(&dr_state);
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
