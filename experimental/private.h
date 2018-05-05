#ifndef PRIVATE_H
#define PRIVATE_H

#include "../include/gl.h"

struct AlignedVector;

AlignedVector* activePolyList();

void initAttributePointers();

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

#endif // PRIVATE_H
