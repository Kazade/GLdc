#pragma once

#include "gl.h"

__BEGIN_DECLS

extern const char* GLDC_VERSION;


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


/* Initialize the GL pipeline. GL will initialize the PVR. */
GLAPI void APIENTRY glKosInit();

typedef struct {
    /* If GL_TRUE, enables pvr autosorting, this *will* break glDepthFunc/glDepthTest */
    GLboolean autosort_enabled;

    /* If GL_TRUE, enables the PVR FSAA */
    GLboolean fsaa_enabled;

    /* The internal format for paletted textures, must be GL_RGBA4 (default) or GL_RGBA8 */
    GLenum internal_palette_format;

    /* Initial capacity of each of the OP, TR and PT lists in vertices */
    GLuint initial_op_capacity;
    GLuint initial_tr_capacity;
    GLuint initial_pt_capacity;
    GLuint initial_immediate_capacity;

} GLdcConfig;


typedef struct {
    GLuint padding0;
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLubyte bgra[4];
    GLuint padding1;
} GLVertexKOS;

GLAPI void APIENTRY glVertexPackColor3fKOS(GLVertexKOS* vertex, float r, float g, float b);
GLAPI void APIENTRY glVertexPackColor4fKOS(GLVertexKOS* vertex, float r, float g, float b, float a);

GLAPI void APIENTRY glKosInitConfig(GLdcConfig* config);

/* Usage:
 *
 * GLdcConfig config;
 * glKosInitConfig(&config);
 *
 * config.autosort_enabled = GL_TRUE;
 *
 * glKosInitEx(&config);
 */
GLAPI void APIENTRY glKosInitEx(GLdcConfig* config);
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
#define GL_SHARED_TEXTURE_PALETTE_4_KOS             0xEF00
#define GL_SHARED_TEXTURE_PALETTE_5_KOS             0xEF01
#define GL_SHARED_TEXTURE_PALETTE_6_KOS             0xEF02
#define GL_SHARED_TEXTURE_PALETTE_7_KOS             0xEF03
#define GL_SHARED_TEXTURE_PALETTE_8_KOS             0xEF04
#define GL_SHARED_TEXTURE_PALETTE_9_KOS             0xEF05

#define GL_SHARED_TEXTURE_PALETTE_10_KOS             0xEF06
#define GL_SHARED_TEXTURE_PALETTE_11_KOS             0xEF07
#define GL_SHARED_TEXTURE_PALETTE_12_KOS             0xEF08
#define GL_SHARED_TEXTURE_PALETTE_13_KOS             0xEF09
#define GL_SHARED_TEXTURE_PALETTE_14_KOS             0xEF0A
#define GL_SHARED_TEXTURE_PALETTE_15_KOS             0xEF0B
#define GL_SHARED_TEXTURE_PALETTE_16_KOS             0xEF0C
#define GL_SHARED_TEXTURE_PALETTE_17_KOS             0xEF0D
#define GL_SHARED_TEXTURE_PALETTE_18_KOS             0xEF0E
#define GL_SHARED_TEXTURE_PALETTE_19_KOS             0xEF0F

#define GL_SHARED_TEXTURE_PALETTE_20_KOS             0xEF10
#define GL_SHARED_TEXTURE_PALETTE_21_KOS             0xEF11
#define GL_SHARED_TEXTURE_PALETTE_22_KOS             0xEF12
#define GL_SHARED_TEXTURE_PALETTE_23_KOS             0xEF13
#define GL_SHARED_TEXTURE_PALETTE_24_KOS             0xEF14
#define GL_SHARED_TEXTURE_PALETTE_25_KOS             0xEF15
#define GL_SHARED_TEXTURE_PALETTE_26_KOS             0xEF16
#define GL_SHARED_TEXTURE_PALETTE_27_KOS             0xEF17
#define GL_SHARED_TEXTURE_PALETTE_28_KOS             0xEF18
#define GL_SHARED_TEXTURE_PALETTE_29_KOS             0xEF19

#define GL_SHARED_TEXTURE_PALETTE_30_KOS             0xEF1A
#define GL_SHARED_TEXTURE_PALETTE_31_KOS             0xEF1B
#define GL_SHARED_TEXTURE_PALETTE_32_KOS             0xEF1C
#define GL_SHARED_TEXTURE_PALETTE_33_KOS             0xEF1D
#define GL_SHARED_TEXTURE_PALETTE_34_KOS             0xEF1E
#define GL_SHARED_TEXTURE_PALETTE_35_KOS             0xEF1F
#define GL_SHARED_TEXTURE_PALETTE_36_KOS             0xEF20
#define GL_SHARED_TEXTURE_PALETTE_37_KOS             0xEF21
#define GL_SHARED_TEXTURE_PALETTE_38_KOS             0xEF22
#define GL_SHARED_TEXTURE_PALETTE_39_KOS             0xEF23

#define GL_SHARED_TEXTURE_PALETTE_40_KOS             0xEF24
#define GL_SHARED_TEXTURE_PALETTE_41_KOS             0xEF25
#define GL_SHARED_TEXTURE_PALETTE_42_KOS             0xEF26
#define GL_SHARED_TEXTURE_PALETTE_43_KOS             0xEF27
#define GL_SHARED_TEXTURE_PALETTE_44_KOS             0xEF28
#define GL_SHARED_TEXTURE_PALETTE_45_KOS             0xEF29
#define GL_SHARED_TEXTURE_PALETTE_46_KOS             0xEF2A
#define GL_SHARED_TEXTURE_PALETTE_47_KOS             0xEF2B
#define GL_SHARED_TEXTURE_PALETTE_48_KOS             0xEF2C
#define GL_SHARED_TEXTURE_PALETTE_49_KOS             0xEF2D

#define GL_SHARED_TEXTURE_PALETTE_50_KOS             0xEF2E
#define GL_SHARED_TEXTURE_PALETTE_51_KOS             0xEF2F
#define GL_SHARED_TEXTURE_PALETTE_52_KOS             0xEF30
#define GL_SHARED_TEXTURE_PALETTE_53_KOS             0xEF31
#define GL_SHARED_TEXTURE_PALETTE_54_KOS             0xEF32
#define GL_SHARED_TEXTURE_PALETTE_55_KOS             0xEF33
#define GL_SHARED_TEXTURE_PALETTE_56_KOS             0xEF34
#define GL_SHARED_TEXTURE_PALETTE_57_KOS             0xEF35
#define GL_SHARED_TEXTURE_PALETTE_58_KOS             0xEF36
#define GL_SHARED_TEXTURE_PALETTE_59_KOS             0xEF37

#define GL_SHARED_TEXTURE_PALETTE_60_KOS             0xEF38
#define GL_SHARED_TEXTURE_PALETTE_61_KOS             0xEF39
#define GL_SHARED_TEXTURE_PALETTE_62_KOS             0xEF3A
#define GL_SHARED_TEXTURE_PALETTE_63_KOS             0xEF3B

/* Pass to glTexParameteri to set the shared bank */
#define GL_SHARED_TEXTURE_BANK_KOS                  0xEF3C

/* Memory allocation extension (GL_KOS_texture_memory_management) */
GLAPI GLvoid APIENTRY glDefragmentTextureMemory_KOS(void);

#define GL_FREE_TEXTURE_MEMORY_KOS                  0xEF3D
#define GL_USED_TEXTURE_MEMORY_KOS                  0xEF3E
#define GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS       0xEF3F

//for palette internal format (glfcConfig)
#define GL_RGB565_KOS                               0xEF40

/**
 * This is a custom extension to OpenGL that allows submitting data
 * directly in the expected format and alignment of the Dreamcast PowerVR
 * chip.
 *
 * Vertices sent to via this API should be 4-byte aligned (ideally 32) and
 * use the following layout:
 *
 * uint32_t padding; // (Space for the PVR command header)
 * float xyz[3]; // Pos
 * float uv[2];  // Tex coord
 * uint8_t bgra[4]; // Vertex color
 * uint8_t obgra[4]; // Offset color (currently unused)
 *
 * This layout is 32 bytes in size and is copied directly into the PVR command stream.
 *
 * However, GL also allows for other attributes: normals, and secondary texture coordinates.
 *
 * For that reason, you can use this API to submit vertices in an "extended" format which
 * is as follows:
 *
 * uint32_t header; // PVR command header
 * float xyz[3]; // Pos
 * float uv[2];  // Tex coord
 * uint8_t bgra[4]; // Vertex color
 * uint8_t obgra[4]; // Offset color (currently unused)
 * float nxyz[3]; // Normal
 * float st[2]; // Secondary texture coords
 * uint32_t padding[3]; // Internal use.
 */

/* Draw polygons by directly copying the vertices into the PVR command stream. Transformation
   will happen as usual. Enabled client state *is NOT* respected.
   ERRORS:

    - GL_INVALID_ENUM is generated if mode is not an accepted value (GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP are accepted)
    - GL_INVALID_VALUE is generated if count is negative
    - GL_INVALID_VALUE is generated if data is NULL
*/
void glDrawPVRArrays32KOS(GLenum mode, GLint first, GLsizei count, void* data);

/* Draw polygons by directly copying the vertices into the PVR command stream. Transformation
   will happen as usual. Enabled client state *is NOT* respected. This uses the extended vertex format which includes
   normals and secondary texture coordinates. These will only take effect if you've enabled secondary texture coordinates
   with glEnable (for ST) or enabled lighting (for normals). GL_NORMALIZE is not respected.

   ERRORS:

    - GL_INVALID_ENUM is generated if mode is not an accepted value (GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP are accepted)
    - GL_INVALID_VALUE is generated if count is negative
    - GL_INVALID_VALUE is generated if data is NULL
*/
void glDrawPVRArrays64KOS(GLenum mode, GLint first, GLsizei count, void* data);


__END_DECLS

