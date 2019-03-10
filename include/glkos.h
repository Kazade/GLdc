#pragma once

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

#define GL_UNSIGNED_BYTE_TWID_KOS                   0xEEFB


GLAPI void APIENTRY glKosSwapBuffers();


/*
 * CUSTOM EXTENSION multiple_shared_palette_KOS
 *
 * This extension allows using up to 4 different shared palettes
 * with ColorTableEXT. The following constants are provided
 * to use as targets for ColorTableExt:
 *
 * - SHARED_TEXTURE_PALETTE_0_KOS
 * - SHARED_TEXTURE_PALETTE_1_KOS
 * - SHARED_TEXTURE_PALETTE_2_KOS
 * - SHARED_TEXTURE_PALETTE_3_KOS
 *
 * In this use case SHARED_TEXTURE_PALETTE_0_KOS is interchangable with SHARED_TEXTURE_PALETTE_EXT
 * (both refer to the first shared palette).
 *
 * To select which palette a texture uses, a new pname is accepted by TexParameteri: SHARED_TEXTURE_BANK_KOS
 * by default textures use shared palette 0.
*/

#define GL_SHARED_TEXTURE_PALETTE_0_KOS             0xEEFC
#define GL_SHARED_TEXTURE_PALETTE_1_KOS             0xEEFD
#define GL_SHARED_TEXTURE_PALETTE_2_KOS             0xEEFE
#define GL_SHARED_TEXTURE_PALETTE_3_KOS             0xEEFF

/* Pass to glTexParameteri to set the shared bank */
#define GL_SHARED_TEXTURE_BANK_KOS                  0xEF00

__END_DECLS

