

#include <kos.h>

#include "../containers/aligned_vector.h"
#include "private.h"
#include "profiler.h"

#define TA_SQ_ADDR (unsigned int *)(void *) \
    (0xe0000000 | (((unsigned long)0x10000000) & 0x03ffffe0))

static PolyList OP_LIST;
static PolyList PT_LIST;
static PolyList TR_LIST;

static void pvr_list_submit(void *src, int n) {
    GLuint *d = TA_SQ_ADDR;
    GLuint *s = src;

    /* fill/write queues as many times necessary */
    while(n--) {
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
        s += CLIP_VERTEX_INT_PADDING;
    }

    /* Wait for both store queues to complete */
    d = (GLuint *)0xe0000000;
    d[0] = d[8] = 0;
}

static void _glInitPVR() {
    pvr_init_params_t params = {
        /* Enable opaque and translucent polygons with size 32 and 32 */
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
        PVR_VERTEX_BUF_SIZE, /* Vertex buffer size */
        0, /* No DMA */
        0, /* No FSAA */
        1 /* Disable translucent auto-sorting to match traditional GL */
    };

    pvr_init(&params);
}


PolyList* _glActivePolyList() {
    if(_glIsBlendingEnabled()) {
        return &TR_LIST;
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


void APIENTRY glKosInit() {
    TRACE();

    _glInitPVR();

    _glInitMatrices();
    _glInitAttributePointers();
    _glInitContext();
    _glInitLights();
    _glInitImmediateMode();
    _glInitFramebuffers();

    _glInitTextures();

    OP_LIST.list_type = PVR_LIST_OP_POLY;
    PT_LIST.list_type = PVR_LIST_PT_POLY;
    TR_LIST.list_type = PVR_LIST_TR_POLY;

    aligned_vector_init(&OP_LIST.vector, sizeof(ClipVertex));
    aligned_vector_init(&PT_LIST.vector, sizeof(ClipVertex));
    aligned_vector_init(&TR_LIST.vector, sizeof(ClipVertex));
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
