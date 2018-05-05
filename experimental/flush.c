

#include <kos.h>

#include "../containers/aligned_vector.h"
#include "private.h"

#define TA_SQ_ADDR (unsigned int *)(void *) \
    (0xe0000000 | (((unsigned long)0x10000000) & 0x03ffffe0))

static AlignedVector OP_LIST;
static AlignedVector PT_LIST;
static AlignedVector TR_LIST;

typedef struct {
    unsigned int cmd[8];
} PVRCommand;

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
    }

    /* Wait for both store queues to complete */
    d = (GLuint *)0xe0000000;
    d[0] = d[8] = 0;
}

static void _initPVR() {
    pvr_init_params_t params = {
        /* Enable opaque and translucent polygons with size 32 and 32 */
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
        GL_PVR_VERTEX_BUF_SIZE, /* Vertex buffer size */
        0, /* No DMA */
        0, /* No FSAA */
        1 /* Disable translucent auto-sorting to match traditional GL */
    };

    pvr_init(&params);
}


AlignedVector* activePolyList() {
    return &OP_LIST;
}

/* FIXME: Rename to glKosInit when experimental is core */
void glKosBoot() {
    _initPVR();
    initAttributePointers();

    aligned_vector_init(&OP_LIST, sizeof(PVRCommand));
    aligned_vector_init(&PT_LIST, sizeof(PVRCommand));
    aligned_vector_init(&TR_LIST, sizeof(PVRCommand));
}

void glKosSwapBuffers() {
    pvr_wait_ready();

    pvr_scene_begin();

        pvr_list_begin(PVR_LIST_OP_POLY);
        pvr_list_submit(OP_LIST.data, OP_LIST.size);
        pvr_list_finish();

        pvr_list_begin(PVR_LIST_PT_POLY);
        pvr_list_submit(PT_LIST.data, PT_LIST.size);
        pvr_list_finish();

        pvr_list_begin(PVR_LIST_TR_POLY);
        pvr_list_submit(TR_LIST.data, TR_LIST.size);
        pvr_list_finish();
    pvr_scene_finish();
}
