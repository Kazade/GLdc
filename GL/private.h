#ifndef PRIVATE_H
#define PRIVATE_H

#include "../include/gl.h"
#include "../containers/aligned_vector.h"
#include "../containers/named_array.h"
#include "./clip.h"

#define TRACE_ENABLED 0
#define TRACE() if(TRACE_ENABLED) {fprintf(stderr, "%s\n", __func__);}

#define VERTEX_ENABLED_FLAG     (1 << 0)
#define UV_ENABLED_FLAG         (1 << 1)
#define ST_ENABLED_FLAG         (1 << 2)
#define DIFFUSE_ENABLED_FLAG    (1 << 3)
#define NORMAL_ENABLED_FLAG     (1 << 4)

#define MAX_TEXTURE_SIZE 1024

#define CLIP_VERTEX_INT_PADDING 6

typedef struct {
    pvr_poly_hdr_t hdr;
    unsigned int padding[CLIP_VERTEX_INT_PADDING];
} PVRHeader;

typedef struct {
    unsigned int flags;      /* Constant PVR_CMD_USERCLIP */
    unsigned int d1, d2, d3; /* Ignored for this type */
    unsigned int sx,         /* Start x */
             sy,         /* Start y */
             ex,         /* End x */
             ey;         /* End y */

    /* Padding to match clip vertex */
    unsigned int padding[CLIP_VERTEX_INT_PADDING];
} PVRTileClipCommand; /* Tile Clip command for the pvr */

typedef struct {
    unsigned int list_type;
    AlignedVector vector;
} PolyList;

typedef struct {
    /* Palette data is always stored in RAM as RGBA8888 and packed as ARGB8888
     * when uploaded to the PVR */
    GLubyte*    data;
    GLushort    width;  /* The user specified width */
    GLushort     size;   /* The size of the bank (16 or 256) */
    GLenum      format;
    GLshort      bank;
} TexturePalette;

typedef struct {
    GLushort width;
    GLushort height;
    GLuint   color; /* This is the PVR texture format */
    GLubyte  env;
    GLushort  mipmap;  /* Bitmask of supplied mipmap levels */
    GLubyte mipmapCount; /* The number of mipmap levels */
    GLubyte  uv_clamp;
    GLuint   index;
    GLvoid *data;
    GLuint dataStride;

    GLenum minFilter;
    GLenum magFilter;

    GLboolean isCompressed;
    GLboolean isPaletted;

    TexturePalette* palette;

    /* When using the shared palette, this is the bank (0-3) */
    GLushort shared_bank;
} TextureObject;

typedef struct {
    GLfloat emissive[4];
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat exponent;
} Material;

typedef struct {
    GLfloat position[4];
    GLfloat spot_direction[3];
    GLfloat spot_cutoff;
    GLfloat constant_attenuation;
    GLfloat linear_attenuation;
    GLfloat quadratic_attenuation;
    GLfloat spot_exponent;
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat ambient[4];
    GLboolean is_directional;
} LightSource;


PolyList *_glActivePolyList();
PolyList *_glTransparentPolyList();

void _glInitAttributePointers();
void _glInitContext();
void _glInitLights();
void _glInitImmediateMode(GLuint initial_size);
void _glInitMatrices();
void _glInitFramebuffers();

void _glMatrixLoadNormal();
void _glMatrixLoadModelView();
void _glMatrixLoadTexture();
void _glApplyRenderMatrix();

matrix_t* _glGetProjectionMatrix();
matrix_t* _glGetModelViewMatrix();

void _glWipeTextureOnFramebuffers(GLuint texture);
GLubyte _glCheckImmediateModeInactive(const char* func);

pvr_poly_cxt_t* _glGetPVRContext();
GLubyte _glInitTextures();

void _glUpdatePVRTextureContext(pvr_poly_cxt_t* context, GLshort textureUnit);

typedef struct {
    const void* ptr;
    GLenum type;
    GLsizei stride;
    GLint size;
} AttribPointer;

GLboolean _glCheckValidEnum(GLint param, GLint* values, const char* func);

GLuint _glGetEnabledAttributes();
AttribPointer* _glGetVertexAttribPointer();
AttribPointer* _glGetDiffuseAttribPointer();
AttribPointer* _glGetNormalAttribPointer();
AttribPointer* _glGetUVAttribPointer();
AttribPointer* _glGetSTAttribPointer();
GLenum _glGetShadeModel();

TextureObject* _glGetTexture0();
TextureObject* _glGetTexture1();
TextureObject* _glGetBoundTexture();
GLubyte _glGetActiveTexture();
GLuint _glGetActiveClientTexture();
TexturePalette* _glGetSharedPalette(GLshort bank);
void _glSetInternalPaletteFormat(GLenum val);

GLboolean _glIsSharedTexturePaletteEnabled();
void _glApplyColorTable(TexturePalette *palette);

GLboolean _glIsBlendingEnabled();
GLboolean _glIsAlphaTestEnabled();

GLboolean _glIsMipmapComplete(const TextureObject* obj);
GLubyte* _glGetMipmapLocation(TextureObject* obj, GLuint level);
GLuint _glGetMipmapLevelCount(TextureObject* obj);

GLboolean _glIsLightingEnabled();
GLboolean _glIsLightEnabled(GLubyte light);
GLboolean _glIsColorMaterialEnabled();
void _glCalculateLightingContribution(const GLint light, const GLfloat* pos, const GLfloat* normal, uint8_t* bgra, GLfloat* colour);

unsigned char _glIsClippingEnabled();
void _glEnableClipping(unsigned char v);

void _glKosThrowError(GLenum error, const char *function);
void _glKosPrintError();
GLubyte _glKosHasError();

#define PVR_VERTEX_BUF_SIZE 2560 * 256
#define MAX_TEXTURE_UNITS 2
#define MAX_LIGHTS 8

#define CLAMP( X, MIN, MAX )  ( (X)<(MIN) ? (MIN) : ((X)>(MAX) ? (MAX) : (X)) )

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
