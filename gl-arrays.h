/* KallistiGL for KallistiOS ##version##

   libgl/gl-arrays.h
   Copyright (C) 2013-2014 Josh Pearson

   Arrays Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS

   Here, it is not necessary to enable or disable client states;
   the API is aware of what arrays have been submitted, and will
   render accordingly.  If you submit a normal pointer, dynamic
   vertex lighting will be applied even if you submit a color
   pointer, so only submit one or the other.

*/

#ifndef GL_ARRAYS_H
#define GL_ARRAYS_H

#include "gl.h"

#define GL_KOS_USE_ARRAY     (1<<0)
#define GL_KOS_USE_TEXTURE0  (1<<1)
#define GL_KOS_USE_TEXTURE1  (1<<2)
#define GL_KOS_USE_COLOR     (1<<3)
#define GL_KOS_USE_NORMAL    (1<<4)

#endif
