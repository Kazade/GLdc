/* KallistiGL for KallistiOS ##version##

   libgl/gl-arrays.h
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Arrays Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS

   Here, it is not necessary to enable or disable client states;
   the API is aware of what arrays have been submitted, and will
   render accordingly.  If you submit a normal pointer, dynamic
   vertex lighting will be applied even if you submit a color
   pointer, so only submit one or the other.

   ToDo: glDrawElements() is not yet implemented.
*/

#ifndef GL_ARRAYS_H
#define GL_ARRAYS_H

#include "gl.h"

#define GL_USE_ARRAY         0x0001
#define GL_USE_TEXTURE       0x0010
#define GL_USE_COLOR         0x0100
#define GL_USE_NORMAL        0x1000
#define GL_USE_TEXTURE_COLOR 0x0111
#define GL_USE_TEXTURE_LIT   0x1011

void (*_glKosArrayTexCoordFunc)(pvr_vertex_t *);
void (*_glKosArrayColorFunc)(pvr_vertex_t *);

void (*_glKosElementTexCoordFunc)(pvr_vertex_t *, GLuint);
void (*_glKosElementColorFunc)(pvr_vertex_t *, GLuint);

static GLfloat *GL_VERTEX_POINTER = NULL;
static GLushort GL_VERTEX_STRIDE = 0;

static GLfloat *GL_NORMAL_POINTER = NULL;
static GLushort GL_NORMAL_STRIDE = 0;

static GLfloat *GL_TEXCOORD_POINTER = NULL;
static GLushort GL_TEXCOORD_STRIDE = 0;

static GLfloat *GL_TEXCOORD2_POINTER = NULL;
static GLushort GL_TEXCOORD2_STRIDE = 0;

static GLfloat *GL_COLOR_POINTER = NULL;
static GLushort GL_COLOR_STRIDE = 0;
static GLubyte GL_COLOR_COMPONENTS = 0;
static GLenum  GL_COLOR_TYPE = 0;

static GLubyte   *GL_INDEX_POINTER_U8 = NULL;
static GLushort   *GL_INDEX_POINTER_U16 = NULL;

#endif
