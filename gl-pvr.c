/* KallistiGL for KallistiOS ##version##

   libgl/gl-pvr.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Vertex Buffer Routines for interfacing the Dreamcast's SH4 CPU and PowerVR GPU.

   What we are doing here is using a Vertex Buffer in main RAM to store the
   submitted data, untill the user finishes the scene by either calling
   glSwapBuffer() to submit the buffer to the PVR for render to the screen OR
   glSwapBufferToTexture() to submit the buffer to the PVR for render to texture.

   This solution means the client can switch between transparent and opaque polygon
   sumbission at any time, in no particular order, with no penalty in speed.

   The size of the Vertex Buffer can be controlled by setting some params on gl-pvr.h:
   GL_PVR_VERTEX_BUF_SIZE controls size of Vertex Buffer in the PVR VRAM
   GL_MAX_VERTS conrols the number of vertices per list in the Vertex Buffer in SH4 RAM
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl.h"
#include "gl-sh4.h"
#include "gl-pvr.h"

/* Vertex Buffer Functions *************************************************************************/

#ifdef GL_USE_MALLOC
static pvr_cmd_t   *GL_VBUF[2] __attribute__((aligned(32)));  /* Dynamic Vertex Buffer */
static pvr_cmd_t   *GL_CBUF;                                  /* Dynamic Clip Buffer */
#else
static pvr_cmd_t    GL_VBUF[2][GL_MAX_VERTS] __attribute__((aligned(32))); /* Static Vertex Buffer */
static pvr_cmd_t    GL_CBUF[GL_MAX_VERTS / 2];                             /* Static Clip Buffer */
#endif

static unsigned int GL_VERTS[2] = {0, 0},
                                  GL_CVERTS = 0,
                                  GL_LIST = GL_LIST_OP;

/* Custom version of sq_cpy from KOS for copying vertex data to the PVR */
static inline void pvr_list_submit(void *src, int n) {
    unsigned int *d = TA_SQ_ADDR;
    unsigned int *s = src;

    /* fill/write queues as many times necessary */
    while(n--) {
        asm("pref @%0" : : "r"(s + 8));  /* prefetch 32 bytes for next loop */
        d[0] = *(s++);
        d[1] = *(s++);
        d[2] = *(s++);
        d[3] = *(s++);
        d[4] = *(s++);
        d[5] = *(s++);
        d[6] = *(s++);
        d[7] = *(s++);
        asm("pref @%0" : : "r"(d));
        d += 8;
    }

    /* Wait for both store queues to complete */
    d = (unsigned int *)0xe0000000;
    d[0] = d[8] = 0;
}

inline void *_glKosClipBufAddress() {
    return &GL_CBUF[0];
}

inline void *_glKosClipBufPointer() {
    return &GL_CBUF[GL_CVERTS];
}

inline void _glKosClipBufIncrement() {
    ++GL_CVERTS;
}

inline void _glKosClipBufAdd(unsigned int count) {
    GL_CVERTS += count;
}

inline void _glKosClipBufReset() {
    GL_CVERTS = 0;
}

inline void _glKosVertexBufSwitchOP() {
    GL_LIST = GL_LIST_OP;
}

inline void _glKosVertexBufSwitchTR() {
    GL_LIST = GL_LIST_TR;
}

inline void *_glKosVertexBufAddress(unsigned char list) {
    return &GL_VBUF[list][0];
}

inline void *_glKosVertexBufPointer() {
    return &GL_VBUF[GL_LIST][GL_VERTS[GL_LIST]];
}

inline void _glKosVertexBufIncrement() {
    ++GL_VERTS[GL_LIST];
}

inline void *_glKosTRVertexBufPointer() {
    return &GL_VBUF[GL_LIST_TR][GL_VERTS[GL_LIST_TR]];
}

inline void _glKosTRVertexBufIncrement() {
    ++GL_VERTS[GL_LIST_TR];
}

inline void _glKosVertexBufAdd(unsigned int count) {
    GL_VERTS[GL_LIST] += count;
}

inline void _glKosTRVertexBufAdd(unsigned int count) {
    GL_VERTS[GL_LIST_TR] += count;
}

inline void _glKosVertexBufDecrement() {
    --GL_VERTS[GL_LIST];
}

inline void _glKosVertexBufReset() {
    GL_VERTS[0] = GL_VERTS[1] = 0;
}

inline unsigned int _glKosVertexBufCount(unsigned char list) {
    return GL_VERTS[list];
}

unsigned char _glKosList() {
    return GL_LIST;
}

inline void _glKosVertexBufCopy(void *dst, void *src, GLuint count) {
    memcpy(dst, src, count * 0x20);
}

static inline void glutSwapBuffer() {
    pvr_list_begin(PVR_LIST_OP_POLY);

    pvr_list_submit(_glKosVertexBufAddress(GL_LIST_OP), _glKosVertexBufCount(GL_LIST_OP));

    pvr_list_finish();

    pvr_list_begin(PVR_LIST_TR_POLY);

    pvr_list_submit(_glKosVertexBufAddress(GL_LIST_TR), _glKosVertexBufCount(GL_LIST_TR));

    pvr_list_finish();

    pvr_scene_finish();
}

void glutSwapBuffers() {
    pvr_wait_ready();

    pvr_scene_begin();

    glutSwapBuffer();

    _glKosVertexBufReset();
}

void glutSwapBuffersToTexture(void *dst, GLsizei *x, GLsizei *y) {
    pvr_wait_ready();

    pvr_scene_begin_txr(dst, x, y);

    glutSwapBuffer();

    _glKosVertexBufReset();
}

void glutCopyBufferToTexture(void *dst, GLsizei *x, GLsizei *y) {
    pvr_wait_ready();

    pvr_scene_begin_txr(dst, x, y);

    glutSwapBuffer();
}

int _glKosInitPVR() {
    pvr_init_params_t params = {

        /* Enable opaque and translucent polygons with size 32 and 32 */
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },

        GL_PVR_VERTEX_BUF_SIZE, /* Vertex buffer size */

        0, /* No DMA */


        0 /* No FSAA */
    };

    pvr_init(&params);
#ifdef GL_USE_DMA
    pvr_dma_init();
#endif

#ifdef GL_USE_MALLOC
    GL_VBUF[0] = memalign(0x20, GL_MAX_VERTS * sizeof(pvr_cmd_t));
    GL_VBUF[1] = memalign(0x20, GL_MAX_VERTS * sizeof(pvr_cmd_t));
    GL_CBUF = malloc((GL_MAX_VERTS / 2) * sizeof(pvr_cmd_t));
#endif

    return 1;
}
