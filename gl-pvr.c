/* KallistiGL for KallistiOS ##version##

   libgl/gl-pvr.c
   Copyright (C) 2013-2014 Josh Pearson

   Vertex Buffer Routines for interfacing the Dreamcast's SH4 CPU and PowerVR GPU.

   What we are doing here is using a Vertex Buffer in main RAM to store the
   submitted data, untill the user finishes the scene by either calling
   glSwapBuffer() to submit the buffer to the PVR for render to the screen OR
   glSwapBufferToTexture() to submit the buffer to the PVR for render to texture.

   This solution means the client can switch between transparent and opaque polygon
   sumbission at any time, in no particular order, with no penalty in speed.

   The size of the Vertex Buffer can be controlled by setting some params on gl-pvr.h:
   GL_PVR_VERTEX_BUF_SIZE controls size of Vertex Buffer in the PVR VRAM
   GL_KOS_MAX_VERTS conrols the number of vertices per list in the Vertex Buffer in SH4 RAM
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl.h"
#include "gl-api.h"
#include "gl-sh4.h"
#include "gl-pvr.h"

/* Vertex Buffer Functions *************************************************************************/

#ifdef GL_KOS_USE_MALLOC
static pvr_cmd_t   *GL_VBUF[2] __attribute__((aligned(32)));  /* Dynamic Vertex Buffer */
static pvr_cmd_t   *GL_CBUF;                                  /* Dynamic Clip Buffer */
static glTexCoord  *GL_UVBUF;                                 /* Dynamic Multi-Texture UV Buffer */
#else
static pvr_cmd_t    GL_VBUF[2][GL_KOS_MAX_VERTS] __attribute__((aligned(32))); /* Static Vertex Buffer */
static pvr_cmd_t    GL_CBUF[GL_KOS_MAX_VERTS / 2];                             /* Static Clip Buffer */
static glTexCoord   GL_UVBUF[GL_KOS_MAX_VERTS / 2];                    /* Static Multi-Texture UV Buffer */
#endif

static GLuint GL_VERTS[2] = {0, 0},
                            GL_CVERTS = 0,
                            GL_UVVERTS = 0,
                            GL_LIST = GL_KOS_LIST_OP;

#define GL_KOS_MAX_MULTITEXTURE_OBJECTS 512

static GL_MULTITEX_OBJECT GL_MTOBJS[GL_KOS_MAX_MULTITEXTURE_OBJECTS];
static GLuint             GL_MTOBJECTS = 0;

/* Custom version of sq_cpy from KOS for copying vertex data to the PVR */
static inline void pvr_list_submit(void *src, int n) {
    GLuint *d = TA_SQ_ADDR;
    GLuint *s = src;

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
    d = (GLuint *)0xe0000000;
    d[0] = d[8] = 0;
}

/* Custom version of sq_cpy from KOS for copying 32bytes of vertex data to the PVR */
static inline void pvr_hdr_submit(const GLuint *src) {
    GLuint *d = TA_SQ_ADDR;

    d[0] = *(src++);
    d[1] = *(src++);
    d[2] = *(src++);
    d[3] = *(src++);
    d[4] = *(src++);
    d[5] = *(src++);
    d[6] = *(src++);
    d[7] = *(src++);
    
    asm("pref @%0" : : "r"(d));
}

inline void _glKosPushMultiTexObject(GL_TEXTURE_OBJECT *tex,
                                     pvr_vertex_t *src,
                                     GLuint count) {
    _glKosCompileHdrMT(&GL_MTOBJS[GL_MTOBJECTS].hdr, tex);

    GL_MTOBJS[GL_MTOBJECTS].src = src;
    GL_MTOBJS[GL_MTOBJECTS++].count = count;
}

inline void _glKosResetMultiTexObject() {
    GL_MTOBJECTS = 0;
}

inline void *_glKosMultiUVBufAddress() {
    return &GL_UVBUF[0];
}

inline void *_glKosMultiUVBufPointer() {
    return &GL_UVBUF[GL_UVVERTS];
}

inline void _glKosMultiUVBufIncrement() {
    ++GL_UVVERTS;
}

inline void _glKosMultiUVBufAdd(GLuint count) {
    GL_UVVERTS += count;
}

inline void _glKosMultiUVBufReset() {
    GL_UVVERTS = 0;
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

inline void _glKosClipBufAdd(GLuint count) {
    GL_CVERTS += count;
}

inline void _glKosClipBufReset() {
    GL_CVERTS = 0;
}

inline void _glKosVertexBufSwitchOP() {
    GL_LIST = GL_KOS_LIST_OP;
}

inline void _glKosVertexBufSwitchTR() {
    GL_LIST = GL_KOS_LIST_TR;
}

inline void *_glKosVertexBufAddress(GLubyte list) {
    return &GL_VBUF[list][0];
}

inline void *_glKosVertexBufPointer() {
    return &GL_VBUF[GL_LIST][GL_VERTS[GL_LIST]];
}

inline void _glKosVertexBufIncrement() {
    ++GL_VERTS[GL_LIST];
}

inline void *_glKosTRVertexBufPointer() {
    return &GL_VBUF[GL_KOS_LIST_TR][GL_VERTS[GL_KOS_LIST_TR]];
}

inline void _glKosTRVertexBufIncrement() {
    ++GL_VERTS[GL_KOS_LIST_TR];
}

inline void _glKosVertexBufAdd(GLuint count) {
    GL_VERTS[GL_LIST] += count;
}

inline void _glKosTRVertexBufAdd(GLuint count) {
    GL_VERTS[GL_KOS_LIST_TR] += count;
}

inline void _glKosVertexBufDecrement() {
    --GL_VERTS[GL_LIST];
}

inline void _glKosVertexBufReset() {
    GL_VERTS[0] = GL_VERTS[1] = 0;
}

inline GLuint _glKosVertexBufCount(GLubyte list) {
    return GL_VERTS[list];
}

GLubyte _glKosList() {
    return GL_LIST;
}

inline void _glKosVertexBufCopy(void *dst, void *src, GLuint count) {
    memcpy(dst, src, count * 0x20);
}

static inline void glutSwapBuffer() {
    pvr_list_begin(PVR_LIST_OP_POLY);
#ifdef GL_KOS_USE_DMA
    pvr_dma_transfer(_glKosVertexBufAddress(GL_KOS_LIST_OP), 0,
                     _glKosVertexBufCount(GL_KOS_LIST_OP) * 32,
                     PVR_DMA_TA, 1, NULL, 0);
#else
    pvr_list_submit(_glKosVertexBufAddress(GL_KOS_LIST_OP), _glKosVertexBufCount(GL_KOS_LIST_OP));
#endif    
    pvr_list_finish();

    pvr_list_begin(PVR_LIST_TR_POLY);
#ifdef GL_KOS_USE_DMA
    pvr_dma_transfer(_glKosVertexBufAddress(GL_KOS_LIST_TR), 0,
                     _glKosVertexBufCount(GL_KOS_LIST_TR) * 32,
                     PVR_DMA_TA, 1, NULL, 0);    
#else
    pvr_list_submit(_glKosVertexBufAddress(GL_KOS_LIST_TR), _glKosVertexBufCount(GL_KOS_LIST_TR));
#endif 
    /* Multi-Texture Pass - Modify U/V coords of submitted vertices */
    GLuint i, v;
    glTexCoord *mt = _glKosMultiUVBufAddress();

    for(i = 0; i < GL_MTOBJECTS; i++) {
        //copy vertex uv
        for(v = 0; v < GL_MTOBJS[i].count; v ++) {
            GL_MTOBJS[i].src[v].u = mt->u;
            GL_MTOBJS[i].src[v].v = mt->v;
            ++mt;
        }

        // submit vertex data to PVR
#ifdef GL_KOS_USE_DMA
        pvr_hdr_submit((GLuint *)&GL_MTOBJS[i].hdr);
        pvr_dma_transfer(GL_MTOBJS[i].src, 0,
                         GL_MTOBJS[i].count * 32, PVR_DMA_TA, 1, NULL, 0);
#else                         
        pvr_list_submit((pvr_poly_hdr_t *)&GL_MTOBJS[i].hdr, 1);
        pvr_list_submit((pvr_vertex_t *)GL_MTOBJS[i].src, GL_MTOBJS[i].count);
#endif
    }

    _glKosResetMultiTexObject(); /* End Multi-Texture Pass */

    pvr_list_finish();

    pvr_scene_finish();
}

void glutSwapBuffers() {
    pvr_wait_ready();

    if(_glKosGetFBO()) {
        GLsizei w = _glKosGetFBOWidth(_glKosGetFBO());
        GLsizei h = _glKosGetFBOHeight(_glKosGetFBO());
        pvr_scene_begin_txr(_glKosGetFBOData(_glKosGetFBO()), &w, &h);
    }
    else
        pvr_scene_begin();

    glutSwapBuffer();

    _glKosVertexBufReset();

    _glKosMultiUVBufReset();

}

void glutCopyBufferToTexture(void *dst, GLsizei *x, GLsizei *y) {
    if(_glKosGetFBO()) {
        pvr_wait_ready();

        pvr_scene_begin_txr(dst, x, y);

        glutSwapBuffer();
    }
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

#ifdef GL_KOS_USE_MALLOC
    GL_VBUF[0] = memalign(0x20, GL_KOS_MAX_VERTS * sizeof(pvr_cmd_t));
    GL_VBUF[1] = memalign(0x20, GL_KOS_MAX_VERTS * sizeof(pvr_cmd_t));
    GL_CBUF = malloc((GL_KOS_MAX_VERTS / 2) * sizeof(pvr_cmd_t));
    GL_UVBUF = malloc(GL_KOS_MAX_VERTS * sizeof(glTexCoord));
#endif

    return 1;
}
