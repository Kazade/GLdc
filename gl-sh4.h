/* KallistiGL for KallistiOS ##version##

   libgl/gl-sh4.h
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Macros for utilizing the Dreamcast's SH4 CPU vector / matrix math functions.
*/

#ifndef GL_SH4_H
#define GL_SH4_H

typedef float vector3f[3];     /* 3 float vector */
typedef float matrix4f[4][4];  /* 4x4 float matrix */

/* DEG2RAD - convert Degrees to Radians = PI / 180.0f */
#define DEG2RAD (0.01745329251994329576923690768489)

/* Calculate Spot Light Angle Cosine = (PI / 180.0f) * (n / 2) */
#define LCOS(n) fcos(n*0.00872664625997164788461845384244)

/* Internal GL API macro */
#define mat_trans_fv12() { \
        __asm__ __volatile__( \
                              "fldi1 fr15\n" \
                              "ftrv	 xmtrx, fv12\n" \
                              "fldi1 fr14\n" \
                              "fdiv	 fr15, fr14\n" \
                              "fmul	 fr14, fr12\n" \
                              "fmul	 fr14, fr13\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z) \
                              : "0" (__x), "1" (__y), "2" (__z) \
                              : "fr15" ); \
    }

/* Internal GL API macro */
#define mat_trans_fv12_nodiv() { \
        __asm__ __volatile__( \
                              "fldi1 fr15\n" \
                              "ftrv	 xmtrx, fv12\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z) \
                              : "0" (__x), "1" (__y), "2" (__z) ); \
    }

#define mat_trans_fv12_nodivw() { \
        __asm__ __volatile__( \
                              "fldi1 fr15\n" \
                              "ftrv	 xmtrx, fv12\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
                              : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
    }

#endif
