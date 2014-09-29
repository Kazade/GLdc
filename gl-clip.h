/* KallistiGL for KallistiOS ##version##

   libgl/gl-clip.h
   Copyright (C) 2013-2014 Josh Pearson

   Near-Z Clipping Algorithm (C) 2013-2014 Josh PH3NOM Pearson
   Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS
   Outputs a mix of Triangles and Triangle Strips for use with the PVR
*/

#ifndef GL_CLIP_H
#define GL_CLIP_H

#include "gl.h"
#include "gl-sh4.h"

#define NONE   0x0000 /* Clip Codes */
#define FIRST  0x0001
#define SECOND 0x0010
#define THIRD  0x0100
#define ALL    0x0111
#define FIRST_TWO_OUT      0x0011
#define FIRST_AND_LAST_OUT 0x0101
#define LAST_TWO_OUT       0x0110

#define ALPHA 0xFF000000 /* Color Components using PVR's Packed 32bit int */
#define RED   0x00FF0000
#define GREEN 0x0000FF00
#define BLUE  0x000000FF

#define CLIP_NEARZ -0.5f /* Clip Threshold */

typedef struct {
    float x, y, z;
} float3;

typedef struct {
    unsigned char b, g, r, a;
} colorui;

#endif
