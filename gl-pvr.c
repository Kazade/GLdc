/* KallistiGL for KallistiOS ##version##

   libgl/gl-pvr.c
   Copyright (C) 2013-2014 Josh Pearson
   Copyright (C) 2016 Lawrence Sebald

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

#include <dc/sq.h>

#include "gl.h"
#include "gl-api.h"
#include "gl-sh4.h"
#include "gl-pvr.h"

/* Vertex Buffer Functions *************************************************************************/

#define COMMANDS_PER_ALLOC ((64 * 1024) / 32) /* 64k memory chunks */

static pvr_cmd_t   *GL_VBUF[2] __attribute__((aligned(32)));  /* Dynamic Vertex Buffer */
static pvr_cmd_t   *GL_CBUF;                                  /* Dynamic Clip Buffer */
static glTexCoord  *GL_UVBUF;                                 /* Dynamic Multi-Texture UV */

static GLuint GL_VERTS[2] = {0, 0};
static GLuint GL_VERTS_ALLOCATED[2] = {0, 0};
static GLuint GL_CVERTS = 0;
static GLuint GL_UVVERTS = 0;
static GLuint GL_LIST = GL_KOS_LIST_OP;

#define GL_KOS_MAX_MULTITEXTURE_OBJECTS 512

static GLuint             GL_MTOBJECTS = 0;

void _glKosResetMultiTexObject() {
    GL_MTOBJECTS = 0;
}

void *_glKosMultiUVBufAddress() {
    return &GL_UVBUF[0];
}

void *_glKosMultiUVBufPointer() {
    return &GL_UVBUF[GL_UVVERTS];
}

void _glKosMultiUVBufIncrement() {
    ++GL_UVVERTS;
}

void _glKosMultiUVBufAdd(GLuint count) {
    GL_UVVERTS += count;
}

void _glKosMultiUVBufReset() {
    GL_UVVERTS = 0;
}

void *_glKosClipBufAddress() {
    return &GL_CBUF[0];
}

void *_glKosClipBufPointer() {
    return &GL_CBUF[GL_CVERTS];
}

void _glKosClipBufIncrement() {
    ++GL_CVERTS;
}

void _glKosClipBufAdd(GLuint count) {
    GL_CVERTS += count;
}

void _glKosClipBufReset() {
    GL_CVERTS = 0;
}

void _glKosVertexBufSwitchOP() {
    GL_LIST = GL_KOS_LIST_OP;
}

void _glKosVertexBufSwitchTR() {
    GL_LIST = GL_KOS_LIST_TR;
}

void *_glKosVertexBufAddress(GLubyte list) {
    return &GL_VBUF[list][0];
}

void *_glKosVertexBufPointer() {
    return &GL_VBUF[GL_LIST][GL_VERTS[GL_LIST]];
}

static void _glKosVertexBufIncrementList(GLubyte list, GLuint count) {
    GL_VERTS[list] += count;

    if(GL_VERTS[list] > GL_VERTS_ALLOCATED[list]) {
        /* Grow the list buffer. We can't use realloc with memalign so we have to memcpy */
        pvr_cmd_t* orig = GL_VBUF[list];

        /* Increase the allocated counter */
        GL_VERTS_ALLOCATED[list] += COMMANDS_PER_ALLOC;

        /* Create a new memory buffer with the new size */
        GL_VBUF[list] = memalign(
            0x20,
            GL_VERTS_ALLOCATED[list] * sizeof(pvr_cmd_t)
        );

        /* Copy across the original data */
        memcpy(GL_VBUF[list], orig, (GL_VERTS[list] - count) * sizeof(pvr_cmd_t));

        /* Free previously allocated memory */
        free(orig);
    }
}

void _glKosVertexBufIncrement() {
    _glKosVertexBufIncrementList(GL_LIST, 1);
}

void *_glKosTRVertexBufPointer() {
    return &GL_VBUF[GL_KOS_LIST_TR][GL_VERTS[GL_KOS_LIST_TR]];
}

void _glKosTRVertexBufIncrement() {
    _glKosVertexBufIncrementList(GL_KOS_LIST_TR, 1);
}

void _glKosVertexBufAdd(GLuint count) {
    _glKosVertexBufIncrementList(GL_LIST, count);
}

void _glKosTRVertexBufAdd(GLuint count) {
    _glKosVertexBufIncrementList(GL_KOS_LIST_TR, count);
}

void _glKosVertexBufDecrement() {
    /* Intentionally don't free data here, we only ever grow */
    --GL_VERTS[GL_LIST];
}

void _glKosVertexBufReset() {
    GL_VERTS[0] = GL_VERTS[1] = 0;
}

GLuint _glKosVertexBufCount(GLubyte list) {
    return GL_VERTS[list];
}

GLubyte _glKosList() {
    return GL_LIST;
}

void _glKosVertexBufCopy(void *dst, void *src, GLuint count) {
    memcpy(dst, src, count * 0x20);
}



