/* KallistiGL for KallistiOS ##version##

   libgl/gl-rgb.h
   Copyright (C) 2013-2015 Josh Pearson

   A set of functions for working with ARGB pixel data.
*/

#ifndef GL_RGB_H
#define GL_RGB_H

typedef GLubyte GLrgb3ub[3];
typedef GLubyte GLrgba4ub[3];
typedef GLfloat GLrgb3f[3];
typedef GLfloat GLrgba4f[4];

#define RGB565_RED_MASK    0xF800
#define RGB565_GREEN_MASK  0x7E0
#define RGB565_BLUE_MASK   0x1F

#define RGB565_RED_SHIFT   0xB
#define RGB565_GREEN_SHIFT 0x5
#define RGB565_BLUE_SHIFT  0x0

#define ARGB4444_ALPHA_MASK 0xF000
#define ARGB4444_RED_MASK   0x0F00
#define ARGB4444_GREEN_MASK 0x00F0
#define ARGB4444_BLUE_MASK  0x000F

#define ARGB4444_ALPHA_SHIFT 0xC
#define ARGB4444_RED_SHIFT   0x8
#define ARGB4444_GREEN_SHIFT 0x4
#define ARGB4444_BLUE_SHIFT  0x0

#define ARGB1555_ALPHA_MASK 0x8000
#define ARGB1555_RED_MASK   0x7C00
#define ARGB1555_GREEN_MASK 0x3E0
#define ARGB1555_BLUE_MASK  0x1F

#define ARGB1555_ALPHA_SHIFT 0xF
#define ARGB1555_RED_SHIFT   0xA
#define ARGB1555_GREEN_SHIFT 0x5
#define ARGB1555_BLUE_SHIFT  0x0

#define ARGB32_ALPHA_MASK 0xFF000000
#define ARGB32_RGB_MASK   0xFFFFFF
#define ARGB32_RED_SHIFT  0x8

#define RGBA32_APLHA_MASK 0xFF
#define RGBA32_RGB_MASK   0xFFFFFF00

#define RGB1_MAX          0x1
#define RGB4_MAX          0xF
#define RGB5_MAX          0x1F
#define RGB6_MAX          0x3F
#define RGB8_MAX          0xFF
#define RGB16_MAX         0xFFFF

#define RGBA32_2_ARGB32(n) (((n & ARGB32_RGB_MASK) << ARGB32_RED_SHIFT) | (n & ARGB32_ALPHA_MASK))

#define ARGB_PACK_RGBF(r,g,b) (0xFF000000 | ((r*0xFF) << 16) | ((g*0xFF)<<8) | (b*0xFF))
#define ARGB_PACK_ARGBF(a,r,g,b) (((a*0xFF) << 24) | ((r*0xFF) << 16) | ((g*0xFF)<<8) | (b*0xFF))

#define S8_NEG_OFT    128 // Absolute Value of Minimum 8bit Signed Range //
#define S16_NEG_OFT 32768 // Absolute Value of Minimum bit Signed Range //

void _glPixelConvertRGB(int format, int w, int h, void * src, uint16 * dst);
void _glPixelConvertRGBA(int format, int w, int h, void * src, uint16 * dst);

#endif
