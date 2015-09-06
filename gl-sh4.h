/* KallistiGL for KallistiOS ##version##

   libgl/gl-sh4.h
   Copyright (C) 2013-2014 Josh Pearson

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

#define mat_trans_texture4(s, t, r, q) { \
        register float __s __asm__("fr4") = (s); \
        register float __t __asm__("fr5") = (t); \
        register float __r __asm__("fr6") = (r); \
        register float __q __asm__("fr7") = (q); \
        __asm__ __volatile__( \
                              "ftrv	xmtrx,fv4\n" \
                              "fldi1	fr6\n" \
                              "fdiv	fr7,fr6\n" \
                              "fmul	fr6,fr4\n" \
                              "fmul	fr6,fr5\n" \
                              : "=f" (__s), "=f" (__t), "=f" (__r) \
                              : "0" (__s), "1" (__t), "2" (__r) \
                              : "fr7" ); \
        s = __s; t = __t; r = __r; \
    }

#define mat_trans_texture2_nomod(s, t, so, to) { \
        register float __s __asm__("fr4") = (s); \
        register float __t __asm__("fr5") = (t); \
        __asm__ __volatile__( \
                              "fldi0	fr6\n" \
                              "fldi1	fr7\n" \
                              "ftrv	xmtrx,fv4\n" \
                              "fldi1	fr6\n" \
                              "fdiv	fr7,fr6\n" \
                              "fmul	fr6,fr4\n" \
                              "fmul	fr6,fr5\n" \
                              : "=f" (__s), "=f" (__t) \
                              : "0" (__s), "1" (__t) \
                              : "fr7" ); \
        so = __s; to = __t; \
    }

#endif
