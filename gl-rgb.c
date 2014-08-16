/* KallistiGL for KallistiOS ##version##

   libgl/gl-rgb.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   A set of functions for working with ARGB pixel data, used by gluBuild2DMipmaps(...).
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
    uint8 A = (ARGB1555_ALPHA(p1) + ARGB1555_ALPHA(p2)) / 4;
    uint8 R = (ARGB1555_RED(p1) + ARGB1555_RED(p2)) / 4;
    uint8 G = (ARGB1555_GREEN(p1) + ARGB1555_GREEN(p2)) / 4;
    uint8 B = (ARGB1555_BLUE(p1) + ARGB1555_BLUE(p2)) / 4;

    return ((A & RGB1_MAX) << ARGB1555_ALPHA_SHIFT) | ((R & RGB5_MAX) << ARGB1555_RED_SHIFT)
           | ((G & RGB5_MAX) << ARGB1555_GREEN_SHIFT) | (B & RGB5_MAX);
}

uint16 __glKosAverageBiPixelARGB4444(uint16 p1, uint16 p2) {
    uint8 A = (ARGB4444_ALPHA(p1) + ARGB4444_ALPHA(p2)) / 4;
    uint8 R = (ARGB4444_RED(p1) + ARGB4444_RED(p2)) / 4;
    uint8 G = (ARGB4444_GREEN(p1) + ARGB4444_GREEN(p2)) / 4;
    uint8 B = (ARGB4444_BLUE(p1) + ARGB4444_BLUE(p2)) / 4;

    return ((A & RGB4_MAX) << ARGB4444_ALPHA_SHIFT) | ((R & RGB4_MAX) << ARGB4444_RED_SHIFT)
           | ((G & RGB4_MAX) << ARGB4444_GREEN_SHIFT) | (B & RGB4_MAX);
}
