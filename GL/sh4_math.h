// ---- sh4_math.h - SH7091 Math Module ----
//
// Version 1.0.8
//
// This file is part of the DreamHAL project, a hardware abstraction library
// primarily intended for use on the SH7091 found in hardware such as the SEGA
// Dreamcast game console.
//
// This math module is hereby released into the public domain in the hope that it
// may prove useful. Now go hit 60 fps! :)
//
// --Moopthehedgehog
//

#pragma once

// 1/x
// (1.0f / sqrt(x)) ^ 2
// This is about 3x faster than fdiv!
static inline __attribute__((always_inline)) float MATH_Invert(float x)
{
  asm volatile ("fsrra %[one_div_sqrt]\n\t"
  "fmul %[one_div_sqrt], %[one_div_sqrt]\n"
  : [one_div_sqrt] "+f" (x) // outputs, "+" means r/w
  : // no inputs
  : // no clobbers
  );

  return x;
}

// 1/sqrt(x)
static inline __attribute__((always_inline)) float MATH_fsrra(float x)
{
  asm volatile ("fsrra %[one_div_sqrt]\n"
  : [one_div_sqrt] "+f" (x) // outputs, "+" means r/w
  : // no inputs
  : // no clobbers
  );

  return x;
}

// a*b+c
static inline __attribute__((always_inline)) float MATH_fmac(float a, float b, float c)
{
  asm volatile ("fmac fr0, %[floatb], %[floatc]\n"
    : [floatc] "+f" (c) // outputs, "+" means r/w
    : "w" (a), [floatb] "f" (b) // inputs
    : // no clobbers
  );

  return c;
}