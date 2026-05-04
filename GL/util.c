#include "private.h"

uint32_t _glPackNormal(const GLfloat* nxyz) {
    uint8_t bx = (uint8_t)((nxyz[0] + 1) * 127.5);
    uint8_t by = (uint8_t)((nxyz[1] + 1) * 127.5);
    uint8_t bz = (uint8_t)((nxyz[2] + 1) * 127.5);
    return (bx << 16) | (by << 8) | bz;
}

void _glUnpackNormal(uint32_t packed, float* nxyz) {
    nxyz[0] = ((packed >> 16) & 0xFF) / 127.5 - 1;
    nxyz[1] = ((packed >> 8) & 0xFF) / 127.5 - 1;
    nxyz[2] = (packed & 0xFF) / 127.5 - 1;
}

half_float_t _glPackHalfFloat(float f) {
    // From BSD licensed code here: https://forums.developer.nvidia.com/t/error-when-trying-to-use-half-fp16/39786/10
    uint32_t ia = *((uint32_t*) &f);
    uint16_t ir;

    ir = (ia >> 16) & 0x8000;
    if((ia & 0x7f800000) == 0x7f800000) {
        if((ia & 0x7fffffff) == 0x7f800000) {
            ir |= 0x7c00; /* infinity */
        } else {
            ir |= 0x7e00 | ((ia >> (24 - 11)) & 0x1ff); /* NaN, quietened */
        }
    } else if((ia & 0x7f800000) >= 0x33000000) {
        int shift = (int)((ia >> 23) & 0xff) - 127;
        if(shift > 15) {
            ir |= 0x7c00; /* infinity */
        } else {
            ia = (ia & 0x007fffff) | 0x00800000; /* extract mantissa */
            if(shift < -14) {                    /* denormal */
                ir |= ia >> (-1 - shift);
                ia = ia << (32 - (-1 - shift));
            } else { /* normal */
                ir |= ia >> (24 - 11);
                ia = ia << (32 - (24 - 11));
                ir = ir + ((14 + shift) << 10);
            }
            /* IEEE-754 round to nearest of even */
            if((ia > 0x80000000) || ((ia == 0x80000000) && (ir & 1))) {
                ir++;
            }
        }
    }
    return ir;
}

float _glUnpackHalfFloat(half_float_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exponent = ((h >> 10) & 0x1F);
    uint32_t mantissa = (h & 0x3FF);

    if(exponent == 0) {
        if(mantissa == 0) {
            return *((float*)(&sign)); // Zero
        } else {
            // Subnormal
            while((mantissa & 0x400) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            exponent++;
            mantissa &= ~0x400;
        }
    } else if(exponent == 31) {
        uint32_t tmp = (sign | 0x7F800000 | (mantissa << 13));
        return *((float*)(&tmp)); // Inf or NaN
    }

    exponent = exponent + (127 - 15);
    mantissa = mantissa << 13;

    uint32_t f_bits = sign | (exponent << 23) | mantissa;
    return *((float*) (&f_bits));
}
