

#include <kos.h>
#include <stdlib.h>

#include "../include/glkos.h"
#include "../containers/aligned_vector.h"
#include "private.h"
#include "profiler.h"
#include "version.h"

#include "flush.h"

#define CLIP_DEBUG 1

#define TA_SQ_ADDR (unsigned int *)(void *) \
    (0xe0000000 | (((unsigned long)0x10000000) & 0x03ffffe0))

static PolyList OP_LIST;
static PolyList PT_LIST;
static PolyList TR_LIST;

static const int STRIDE = sizeof(Vertex) / sizeof(GLuint);

#define CLIP_TO_PLANE(vert1, vert2) \
    do { \
        float t = _glClipLineToNearZ((vert1), (vert2), out); \
        interpolateVec2((vert1)->uv, (vert2)->uv, t, out->uv); \
        interpolateVec3((vert1)->nxyz, (vert2)->nxyz, t, out->nxyz); \
        interpolateVec2((vert1)->st, (vert2)->st, t, out->st); \
        interpolateColour((vert1)->bgra, (vert2)->bgra, t, out->bgra); \
    } while(0); \


GL_FORCE_INLINE float _glClipLineToNearZ(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    const float d0 = v1->w;
    const float d1 = v2->w;

    assert(isVisible(v1) ^ isVisible(v2));

#if 0
    /* FIXME: Disabled until determined necessary */

    /* We need to shift 't' a little, to avoid the possibility that a
     * rounding error leaves the new vertex behind the near plane. We shift
     * according to the direction we're clipping across the plane */
    const float epsilon = (d0 < d1) ? 0.000001 : -0.000001;
#else
    const float epsilon = 0;
#endif

    float t = MATH_Fast_Divide(d0, (d0 - d1))+ epsilon;

    vout->xyz[0] = MATH_fmac(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = MATH_fmac(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = MATH_fmac(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);
    vout->w = MATH_fmac(v2->w - v1->w, t, v1->w);

#if CLIP_DEBUG
    printf(
        "(%f, %f, %f, %f) -> %f -> (%f, %f, %f, %f) = (%f, %f, %f, %f)\n",
        v1->xyz[0], v1->xyz[1], v1->xyz[2], v1->w, t,
        v2->xyz[0], v2->xyz[1], v2->xyz[2], v2->w,
        vout->xyz[0], vout->xyz[1], vout->xyz[2], vout->w
    );
#endif

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

static Vertex* interpolate_vertex(const Vertex* v0, const Vertex* v1, Vertex* out) {
    /* If v0 is in front of the near plane, and v1 is behind the near plane, this
     * generates a vertex *on* the near plane */
    CLIP_TO_PLANE(v0, v1);
    return out;
}

GL_FORCE_INLINE ListIterator* header_reset(ListIterator* it, Vertex* header) {
    it->it = header;
    it->visibility = 0;
    it->triangle_count = 0;
    it->stack_idx = -1;
    return it;
}

GL_FORCE_INLINE Vertex* current_postinc(ListIterator* it) {
    if(it->remaining == 0) {
        return NULL;
    }

    it->remaining--;
    Vertex* current = it->current;
    it->current++;
    return current;
}

GL_FORCE_INLINE Vertex* push_stack(ListIterator* it) {
#if CLIP_DEBUG
    printf("Using stack: %d\n", it->stack_idx + 1);
#endif

    assert(it->stack_idx + 1 < MAX_STACK);

    return &it->stack[++it->stack_idx];
}

GL_FORCE_INLINE GLboolean shift(ListIterator* it, Vertex* new_vertex) {
    /*
     * Shifts in a new vertex, dropping the oldest. If
     * new_vertex is NULL it will return GL_FALSE (but still
     * shift) */
    it->triangle_count++;
    if(it->triangle_count > 3) it->triangle_count = 3;

    it->triangle[0] = it->triangle[1];
    it->triangle[1] = it->triangle[2];
    it->triangle[2] = new_vertex;

    it->visibility <<= 1;
    it->visibility &= 7;
    it->visibility += isVisible(new_vertex);

    return new_vertex != NULL;
}

ListIterator* _glIteratorNext(ListIterator* it) {
    /* None remaining in the list, and the stack is empty */
    if(!it->remaining && it->stack_idx == -1) {
        return NULL;
    }

    /* Return any vertices we generated */
    if(it->stack_idx > -1) {
#if CLIP_DEBUG
        printf("Yielding stack: %d\n", it->stack_idx);
#endif

        it->it = &it->stack[it->stack_idx--];
        return it;
    }

    if(!isVertex(it->current)) {
        return header_reset(it, current_postinc(it));
    } else {
        /* Make sure we have a full triangle of vertices */
        while(it->triangle_count < 3) {
            if(!isVertex(it->current)) {
                return header_reset(it, current_postinc(it));
            }

            if(!shift(it, current_postinc(it))) {
                /* We reached the end so just
                 * return the oldest until they're gone */
                it->it = it->triangle[0];
                printf("Bailing early!\n");
                return (it->it) ? it : NULL;
            }
        }

        /* OK, by this point we should have info for a complete triangle
         * including visibility */
        switch(it->visibility) {
            case B111:
                /* Totally visible, return the first vertex */
                it->it = it->triangle[0];
                return it;
            break;
            case B100: {
                /* First visible only */
                Vertex* gen2 = push_stack(it);
                Vertex* gen1 = push_stack(it);

                /* Make sure we transfer the flags.. we don't
                 * want to disrupt the strip */
                gen1->flags = it->triangle[1]->flags;
                gen2->flags = it->triangle[2]->flags;

                interpolate_vertex(it->triangle[0], it->triangle[1], gen1);
                interpolate_vertex(it->triangle[0], it->triangle[2], gen2);
                it->visibility = B111; /* All visible now, yay! */

                assert(isVisible(gen1));
                assert(isVisible(gen2));
                assert(isVertex(gen1));
                assert(isVertex(gen2));

                it->it = it->triangle[0];

                /* We're returning v0, and we've pushed
                 * v1 and v2 to the stack, so next time
                 * around we'll need to consume and shift
                 * the next vertex from the source list */
                it->triangle_count--;
                return it;
            } break;
        }
    }

    return NULL;
}

GL_FORCE_INLINE void perspective_divide(Vertex* vertex) {
    float f = MATH_Fast_Invert(vertex->w);
    vertex->xyz[0] *= f;
    vertex->xyz[1] *= f;
    vertex->xyz[2] *= f;
    vertex->xyz[2] = MAX(1.0f - (vertex->xyz[2] * 0.5f + 0.5f), 0.0001f);
}

static void pvr_list_submit(void *src, int n) {
    GLuint *d = TA_SQ_ADDR;

    /* First entry is assumed to always be a header and therefore
     * always submitted (e.g. not clipped) */

    ListIterator* it = _glIteratorBegin(src, n);
    /* fill/write queues as many times necessary */
    while(it) {
        __asm__("pref @%0" : : "r"(it->current + 1));  /* prefetch 64 bytes for next loop */

        if(isVertex(it->current)) {
            perspective_divide(it->current);
        }

        GLuint* s = (GLuint*) it->current;

        d[0] = *(s++);
        d[1] = *(s++);
        d[2] = *(s++);
        d[3] = *(s++);
        d[4] = *(s++);
        d[5] = *(s++);
        d[6] = *(s++);
        d[7] = *(s++);

        /* This prefetch actually commits 32 bytes to the SQ */
        __asm__("pref @%0" : : "r"(d));

        d += 8; /* Move to the next SQ address */

        it = _glIteratorNext(it);
    }

    /* Wait for both store queues to complete */
    d = (GLuint *)0xe0000000;
    d[0] = d[8] = 0;

    free(it);
}

static void _glInitPVR(GLboolean autosort, GLboolean fsaa) {
    pvr_init_params_t params = {
        /* Enable opaque and translucent polygons with size 32 and 32 */
        {PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32},
        PVR_VERTEX_BUF_SIZE, /* Vertex buffer size */
        0, /* No DMA */
        fsaa,
        (autosort) ? 0 : 1
    };

    pvr_init(&params);
}


PolyList* _glActivePolyList() {
    if(_glIsBlendingEnabled()) {
        return &TR_LIST;
    } else if(_glIsAlphaTestEnabled()) {
        return &PT_LIST;
    } else {
        return &OP_LIST;
    }
}

PolyList *_glTransparentPolyList() {
    return &TR_LIST;
}

void APIENTRY glFlush() {

}

void APIENTRY glFinish() {

}


void APIENTRY glKosInitConfig(GLdcConfig* config) {
    config->autosort_enabled = GL_FALSE;
    config->fsaa_enabled = GL_FALSE;

    config->initial_op_capacity = 1024;
    config->initial_pt_capacity = 512;
    config->initial_tr_capacity = 1024;
    config->initial_immediate_capacity = 1024;
    config->internal_palette_format = GL_RGBA4;
}

void APIENTRY glKosInitEx(GLdcConfig* config) {
    TRACE();

    printf("\nWelcome to GLdc! Git revision: %s\n\n", GLDC_VERSION);

    _glInitPVR(config->autosort_enabled, config->fsaa_enabled);

    _glInitMatrices();
    _glInitAttributePointers();
    _glInitContext();
    _glInitLights();
    _glInitImmediateMode(config->initial_immediate_capacity);
    _glInitFramebuffers();

    _glSetInternalPaletteFormat(config->internal_palette_format);

    _glInitTextures();

    OP_LIST.list_type = PVR_LIST_OP_POLY;
    PT_LIST.list_type = PVR_LIST_PT_POLY;
    TR_LIST.list_type = PVR_LIST_TR_POLY;

    aligned_vector_init(&OP_LIST.vector, sizeof(Vertex));
    aligned_vector_init(&PT_LIST.vector, sizeof(Vertex));
    aligned_vector_init(&TR_LIST.vector, sizeof(Vertex));

    aligned_vector_reserve(&OP_LIST.vector, config->initial_op_capacity);
    aligned_vector_reserve(&PT_LIST.vector, config->initial_pt_capacity);
    aligned_vector_reserve(&TR_LIST.vector, config->initial_tr_capacity);
}

void APIENTRY glKosInit() {
    GLdcConfig config;
    glKosInitConfig(&config);
    glKosInitEx(&config);
}

#define QACRTA ((((unsigned int)0x10000000)>>26)<<2)&0x1c

void APIENTRY glKosSwapBuffers() {
    static int frame_count = 0;

    TRACE();

    profiler_push(__func__);

    pvr_wait_ready();

    pvr_scene_begin();
        QACR0 = QACRTA;
        QACR1 = QACRTA;

        pvr_list_begin(PVR_LIST_OP_POLY);
        pvr_list_submit(OP_LIST.vector.data, OP_LIST.vector.size);
        pvr_list_finish();

        pvr_list_begin(PVR_LIST_PT_POLY);
        pvr_list_submit(PT_LIST.vector.data, PT_LIST.vector.size);
        pvr_list_finish();

        pvr_list_begin(PVR_LIST_TR_POLY);
        pvr_list_submit(TR_LIST.vector.data, TR_LIST.vector.size);
        pvr_list_finish();
    pvr_scene_finish();

    aligned_vector_clear(&OP_LIST.vector);
    aligned_vector_clear(&PT_LIST.vector);
    aligned_vector_clear(&TR_LIST.vector);

    profiler_checkpoint("scene");
    profiler_pop();

    if(frame_count++ > 100) {
        profiler_print_stats();
        frame_count = 0;
    }
}
