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
    GLushort     width;
    GLenum      format;
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


PolyList *activePolyList();
PolyList *transparentPolyList();

void initAttributePointers();
void _glInitContext();
void _glInitLights();
void initImmediateMode();
void initMatrices();
void initFramebuffers();

void _matrixLoadNormal();
void _matrixLoadModelView();
void _matrixLoadTexture();
void _applyRenderMatrix();

matrix_t* _glGetProjectionMatrix();

void wipeTextureOnFramebuffers(GLuint texture);
GLubyte checkImmediateModeInactive(const char* func);

pvr_poly_cxt_t* getPVRContext();
GLubyte _glKosInitTextures();

void _glUpdatePVRTextureContext(pvr_poly_cxt_t* context, GLshort textureUnit);

typedef struct {
    const void* ptr;
    GLenum type;
    GLsizei stride;
    GLint size;
} AttribPointer;

GLboolean _glCheckValidEnum(GLint param, GLenum* values, const char* func);

GLuint _glGetEnabledAttributes();
AttribPointer* _glGetVertexAttribPointer();
AttribPointer* _glGetDiffuseAttribPointer();
AttribPointer* _glGetNormalAttribPointer();
AttribPointer* _glGetUVAttribPointer();
AttribPointer* _glGetSTAttribPointer();
GLenum _glGetShadeModel();

TextureObject* getTexture0();
TextureObject* getTexture1();
TextureObject* getBoundTexture();
GLubyte _glGetActiveTexture();
GLuint _glGetActiveClientTexture();

GLboolean _glIsSharedTexturePaletteEnabled();
void _glApplyColorTable();

GLboolean isBlendingEnabled();
GLboolean _glIsMipmapComplete(const TextureObject* obj);
GLubyte* _glGetMipmapLocation(TextureObject* obj, GLuint level);
GLuint _glGetMipmapLevelCount(TextureObject* obj);

GLboolean isLightingEnabled();
GLboolean isLightEnabled(GLubyte light);
GLboolean _glIsColorMaterialEnabled();
void _glCalculateLightingContribution(const GLint light, const GLfloat* pos, const GLfloat* normal, uint8_t* bgra, GLfloat* colour);

unsigned char isClippingEnabled();
void enableClipping(unsigned char v);

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
