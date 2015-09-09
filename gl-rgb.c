/* KallistiGL for KallistiOS ##version##

   libgl/gl-rgb.c
   Copyright (C) 2013-2015 Josh Pearson

   A set of functions for working with ARGB pixel data, used by gluBuild2DMipmaps(...).
   The pixel conversion functions are for submitting textures not supported by the PVR.
*/

#include "gl.h"
#include "gl-rgb.h"

//===================================================================================================//
//== ARGB Bit Masks ==//

static unsigned char ARGB1555_ALPHA(unsigned short c) {
    return (c & ARGB1555_ALPHA_MASK) >> ARGB1555_ALPHA_SHIFT;
}

static unsigned char ARGB1555_RED(unsigned short c) {
    return (c & ARGB1555_RED_MASK) >> ARGB1555_RED_SHIFT;
}

static unsigned char ARGB1555_GREEN(unsigned short c) {
    return (c & ARGB1555_GREEN_MASK) >> ARGB1555_GREEN_SHIFT;
}

static unsigned char ARGB1555_BLUE(unsigned short c) {
    return (c & ARGB1555_BLUE_MASK) >> ARGB1555_BLUE_SHIFT;
}

static unsigned char ARGB4444_ALPHA(unsigned short c) {
    return (c & ARGB4444_ALPHA_MASK) >> ARGB4444_ALPHA_SHIFT;
}

static unsigned char ARGB4444_RED(unsigned short c) {
    return (c & ARGB4444_RED_MASK) >> ARGB4444_RED_SHIFT;
}

static unsigned char ARGB4444_GREEN(unsigned short c) {
    return (c & ARGB4444_GREEN_MASK) >> ARGB4444_GREEN_SHIFT;
}

static unsigned char ARGB4444_BLUE(unsigned short c) {
    return (c & ARGB4444_BLUE_MASK) >> ARGB4444_BLUE_SHIFT;
}

static unsigned char RGB565_RED(unsigned short c) {
    return (c & RGB565_RED_MASK) >> RGB565_RED_SHIFT;
}

static unsigned char RGB565_GREEN(unsigned short c) {
    return (c & RGB565_GREEN_MASK) >> RGB565_GREEN_SHIFT;
}

static unsigned char RGB565_BLUE(unsigned short c) {
    return c & RGB565_BLUE_MASK;
}

//===================================================================================================//
//== Block Compression ==//

uint16 __glKosAverageQuadPixelRGB565(uint16 p1, uint16 p2, uint16 p3, uint16 p4) {
    uint8 R = (RGB565_RED(p1) + RGB565_RED(p2) + RGB565_RED(p3) + RGB565_RED(p4)) / 4;
    uint8 G = (RGB565_GREEN(p1) + RGB565_GREEN(p2) + RGB565_GREEN(p3) + RGB565_GREEN(p4)) / 4;
    uint8 B = (RGB565_BLUE(p1) + RGB565_BLUE(p2) + RGB565_BLUE(p3) + RGB565_BLUE(p4)) / 4;

    return R << RGB565_RED_SHIFT | G << RGB565_GREEN_SHIFT | B;
}

uint16 __glKosAverageQuadPixelARGB1555(uint16 p1, uint16 p2, uint16 p3, uint16 p4) {
    uint8 A = (ARGB1555_ALPHA(p1) + ARGB1555_ALPHA(p2) + ARGB1555_ALPHA(p3) + ARGB1555_ALPHA(p4)) / 4;
    uint8 R = (ARGB1555_RED(p1) + ARGB1555_RED(p2) + ARGB1555_RED(p3) + ARGB1555_RED(p4)) / 4;
    uint8 G = (ARGB1555_GREEN(p1) + ARGB1555_GREEN(p2) + ARGB1555_GREEN(p3) + ARGB1555_GREEN(p4)) / 4;
    uint8 B = (ARGB1555_BLUE(p1) + ARGB1555_BLUE(p2) + ARGB1555_BLUE(p3) + ARGB1555_BLUE(p4)) / 4;

    return ((A & RGB1_MAX) << ARGB1555_ALPHA_SHIFT) | ((R & RGB5_MAX) << ARGB1555_RED_SHIFT)
           | ((G & RGB5_MAX) << ARGB1555_GREEN_SHIFT) | (B & RGB5_MAX);
}

uint16 __glKosAverageQuadPixelARGB4444(uint16 p1, uint16 p2, uint16 p3, uint16 p4) {
    uint8 A = (ARGB4444_ALPHA(p1) + ARGB4444_ALPHA(p2) + ARGB4444_ALPHA(p3) + ARGB4444_ALPHA(p4)) / 4;
    uint8 R = (ARGB4444_RED(p1) + ARGB4444_RED(p2) + ARGB4444_RED(p3) + ARGB4444_RED(p4)) / 4;
    uint8 G = (ARGB4444_GREEN(p1) + ARGB4444_GREEN(p2) + ARGB4444_GREEN(p3) + ARGB4444_GREEN(p4)) / 4;
    uint8 B = (ARGB4444_BLUE(p1) + ARGB4444_BLUE(p2) + ARGB4444_BLUE(p3) + ARGB4444_BLUE(p4)) / 4;

    return ((A & RGB4_MAX) << ARGB4444_ALPHA_SHIFT) | ((R & RGB4_MAX) << ARGB4444_RED_SHIFT)
           | ((G & RGB4_MAX) << ARGB4444_GREEN_SHIFT) | (B & RGB4_MAX);
}

uint16 __glKosAverageBiPixelRGB565(uint16 p1, uint16 p2) {
    uint8 R = (RGB565_RED(p1) + RGB565_RED(p2)) / 2;
    uint8 G = (RGB565_GREEN(p1) + RGB565_GREEN(p2)) / 2;
    uint8 B = (RGB565_BLUE(p1) + RGB565_BLUE(p2)) / 2;

    return R << RGB565_RED_SHIFT | G << RGB565_GREEN_SHIFT | B;
}

uint16 __glKosAverageBiPixelARGB1555(uint16 p1, uint16 p2) {
    uint8 A = (ARGB1555_ALPHA(p1) + ARGB1555_ALPHA(p2)) / 2;
    uint8 R = (ARGB1555_RED(p1) + ARGB1555_RED(p2)) / 2;
    uint8 G = (ARGB1555_GREEN(p1) + ARGB1555_GREEN(p2)) / 2;
    uint8 B = (ARGB1555_BLUE(p1) + ARGB1555_BLUE(p2)) / 2;

    return ((A & RGB1_MAX) << ARGB1555_ALPHA_SHIFT) | ((R & RGB5_MAX) << ARGB1555_RED_SHIFT)
           | ((G & RGB5_MAX) << ARGB1555_GREEN_SHIFT) | (B & RGB5_MAX);
}

uint16 __glKosAverageBiPixelARGB4444(uint16 p1, uint16 p2) {
    uint8 A = (ARGB4444_ALPHA(p1) + ARGB4444_ALPHA(p2)) / 2;
    uint8 R = (ARGB4444_RED(p1) + ARGB4444_RED(p2)) / 2;
    uint8 G = (ARGB4444_GREEN(p1) + ARGB4444_GREEN(p2)) / 2;
    uint8 B = (ARGB4444_BLUE(p1) + ARGB4444_BLUE(p2)) / 2;

    return ((A & RGB4_MAX) << ARGB4444_ALPHA_SHIFT) | ((R & RGB4_MAX) << ARGB4444_RED_SHIFT)
           | ((G & RGB4_MAX) << ARGB4444_GREEN_SHIFT) | (B & RGB4_MAX);
}

//===================================================================================================//
//== Colorspace Conversion ==//

static uint16 _glConvPixelRGBAU32(uint8 r, uint8 g, uint8 b, uint8 a) {
    return (uint16)((a & RGB4_MAX) << ARGB4444_ALPHA_SHIFT) |
           ((r & RGB4_MAX) << ARGB4444_RED_SHIFT) |
           ((g & RGB4_MAX) << ARGB4444_GREEN_SHIFT) |
           ((b & RGB4_MAX));
}

static uint16 _glConvPixelRGBU24(uint8 r, uint8 g, uint8 b) {
    return (uint16)((r & RGB5_MAX) << RGB565_RED_SHIFT) |
           ((g & RGB6_MAX) << RGB565_GREEN_SHIFT) |
           ((b & RGB5_MAX));
}

static void _glConvPixelsRGBF(int w, int h, float *src, uint16 *dst) {
    int i;

    for(i = 0; i < w * h; i++) {
        dst[i] = _glConvPixelRGBU24((uint8)(src[i * 3 + 0] * RGB5_MAX),
                                    (uint8)(src[i * 3 + 1] * RGB6_MAX),
                                    (uint8)(src[i * 3 + 2] * RGB5_MAX));
    }
}

static void _glConvPixelsRGBAF(int w, int h, float *src, uint16 *dst) {
    int i;

    for(i = 0; i < w * h; i++) {
        dst[i] = _glConvPixelRGBAU32((uint8)(src[i * 4 + 0] * RGB4_MAX),
                                     (uint8)(src[i * 4 + 1] * RGB4_MAX),
                                     (uint8)(src[i * 4 + 2] * RGB4_MAX),
                                     (uint8)(src[i * 4 + 3] * RGB4_MAX));
    }
}

static void _glConvPixelsRGBU24(int w, int h, uint8 *src, uint16 *dst) {
    unsigned char r, g, b;
    int i;

    for(i = 0; i < w * h; i++) {
        r = (src[i * 3 + 0] * RGB5_MAX) / RGB8_MAX;
        g = (src[i * 3 + 1] * RGB6_MAX) / RGB8_MAX;
        b = (src[i * 3 + 2] * RGB5_MAX) / RGB8_MAX;

        dst[i] = _glConvPixelRGBU24(r, g, b);
    }
}

static void _glConvPixelsRGBAU32(int w, int h, uint8 *src, uint16 *dst) {
    unsigned char r, g, b, a;
    int i;

    for(i = 0; i < w * h; i++) {
        r = (src[i * 4 + 0] * RGB4_MAX) / RGB8_MAX;
        g = (src[i * 4 + 1] * RGB4_MAX) / RGB8_MAX;
        b = (src[i * 4 + 2] * RGB4_MAX) / RGB8_MAX;
        a = (src[i * 4 + 3] * RGB4_MAX) / RGB8_MAX;

        dst[i] = _glConvPixelRGBAU32(r, g, b, a);
    }
}

static void _glConvPixelsRGBS24(int w, int h, int8 *src, uint16 *dst) {
    unsigned char r, g, b;
    int i;

    for(i = 0; i < w * h; i++) {
        r = ((src[i * 3 + 0] + S8_NEG_OFT) * RGB5_MAX) / RGB8_MAX;
        g = ((src[i * 3 + 1] + S8_NEG_OFT) * RGB6_MAX) / RGB8_MAX;
        b = ((src[i * 3 + 2] + S8_NEG_OFT) * RGB5_MAX) / RGB8_MAX;

        dst[i] = _glConvPixelRGBU24(r, g, b);
    }
}

static void _glConvPixelsRGBAS32(int w, int h, int8 *src, uint16 *dst) {
    unsigned char r, g, b, a;
    int i;

    for(i = 0; i < w * h; i++) {
        r = ((src[i * 4 + 0] + S8_NEG_OFT) * RGB4_MAX) / RGB8_MAX;
        g = ((src[i * 4 + 1] + S8_NEG_OFT) * RGB4_MAX) / RGB8_MAX;
        b = ((src[i * 4 + 2] + S8_NEG_OFT) * RGB4_MAX) / RGB8_MAX;
        a = ((src[i * 4 + 3] + S8_NEG_OFT) * RGB4_MAX) / RGB8_MAX;

        dst[i] = _glConvPixelRGBAU32(r, g, b, a);
    }
}

static void _glConvPixelsRGBS48(int w, int h, int16 *src, uint16 *dst) {
    unsigned char r, g, b;
    int i;

    for(i = 0; i < w * h; i++) {
        r = ((src[i * 3 + 0] + S16_NEG_OFT) * RGB5_MAX) / RGB16_MAX;
        g = ((src[i * 3 + 1] + S16_NEG_OFT) * RGB6_MAX) / RGB16_MAX;
        b = ((src[i * 3 + 2] + S16_NEG_OFT) * RGB5_MAX) / RGB16_MAX;

        dst[i] = _glConvPixelRGBU24(r, g, b);
    }
}

static void _glConvPixelsRGBAS64(int w, int h, int16 *src, uint16 *dst) {
    unsigned char r, g, b, a;
    int i;

    for(i = 0; i < w * h; i++) {
        r = ((src[i * 4 + 0] + S16_NEG_OFT) * RGB4_MAX) / RGB16_MAX;
        g = ((src[i * 4 + 1] + S16_NEG_OFT) * RGB4_MAX) / RGB16_MAX;
        b = ((src[i * 4 + 2] + S16_NEG_OFT) * RGB4_MAX) / RGB16_MAX;
        a = ((src[i * 4 + 3] + S16_NEG_OFT) * RGB4_MAX) / RGB16_MAX;

        dst[i] = _glConvPixelRGBAU32(r, g, b, a);
    }
}

static void _glConvPixelsRGBU48(int w, int h, uint16 *src, uint16 *dst) {
    unsigned char r, g, b;
    int i;

    for(i = 0; i < w * h; i++) {
        r = ((src[i * 3 + 0]) * RGB5_MAX) / RGB16_MAX;
        g = ((src[i * 3 + 1]) * RGB6_MAX) / RGB16_MAX;
        b = ((src[i * 3 + 2]) * RGB5_MAX) / RGB16_MAX;

        dst[i] = _glConvPixelRGBU24(r, g, b);
    }
}

static void _glConvPixelsRGBAU64(int w, int h, uint16 *src, uint16 *dst) {
    unsigned char r, g, b, a;
    int i;

    for(i = 0; i < w * h; i++) {
        r = (src[i * 4 + 0] * RGB4_MAX) / RGB16_MAX;
        g = (src[i * 4 + 1] * RGB4_MAX) / RGB16_MAX;
        b = (src[i * 4 + 2] * RGB4_MAX) / RGB16_MAX;
        a = (src[i * 4 + 3] * RGB4_MAX) / RGB16_MAX;

        dst[i] = _glConvPixelRGBAU32(r, g, b, a);
    }
}

void _glPixelConvertRGB(int format, int w, int h, void *src, uint16 *dst) {
    switch(format) {
        case GL_BYTE:
            _glConvPixelsRGBS24(w, h, (int8 *)src, dst);
            break;

        case GL_UNSIGNED_BYTE:
            _glConvPixelsRGBU24(w, h, (uint8 *)src, dst);
            break;

        case GL_SHORT:
            _glConvPixelsRGBS48(w, h, (int16 *)src, dst);
            break;

        case GL_UNSIGNED_SHORT:
            _glConvPixelsRGBU48(w, h, (uint16 *)src, dst);
            break;

        case GL_FLOAT:
            _glConvPixelsRGBF(w, h, (float *)src, dst);
            break;
    }
}

void _glPixelConvertRGBA(int format, int w, int h, void *src, uint16 *dst) {
    switch(format) {
        case GL_BYTE:
            _glConvPixelsRGBAS32(w, h, (int8 *)src, dst);
            break;

        case GL_UNSIGNED_BYTE:
            _glConvPixelsRGBAU32(w, h, (uint8 *)src, dst);
            break;

        case GL_SHORT:
            _glConvPixelsRGBAS64(w, h, (int16 *)src, dst);
            break;

        case GL_UNSIGNED_SHORT:
            _glConvPixelsRGBAU64(w, h, (uint16 *)src, dst);
            break;

        case GL_FLOAT:
            _glConvPixelsRGBAF(w, h, (float *)src, dst);
            break;
    }
}
