#ifndef GLKOS_H
#define GLKOS_H

#include "gl.h"

__BEGIN_DECLS

/*
 * Dreamcast specific compressed + twiddled formats.
 * We use constants from the range 0xEEE0 onwards
 * to avoid trampling any real GL constants (this is in the middle of the
 * any_vendor_future_use range defined in the GL enum.spec file.
*/
#define GL_UNSIGNED_SHORT_5_6_5_TWID_KOS            0xEEE0
#define GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS      0xEEE2
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS      0xEEE3

#define GL_COMPRESSED_RGB_565_VQ_KOS                0xEEE4
#define GL_COMPRESSED_ARGB_1555_VQ_KOS              0xEEE6
#define GL_COMPRESSED_ARGB_4444_VQ_KOS              0xEEE7

#define GL_COMPRESSED_RGB_565_VQ_TWID_KOS           0xEEE8
#define GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS         0xEEEA
#define GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS         0xEEEB

#define GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS                0xEEEC
#define GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS              0xEEED
#define GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS              0xEEEE

#define GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS           0xEEEF
#define GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS         0xEEF0
#define GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS         0xEEF1

#define GL_NEARZ_CLIPPING_KOS                       0xEEFA


GLAPI void APIENTRY glKosSwapBuffers();



__END_DECLS

#endif // GLKOS_H
