#ifndef PRIVATE_H
#define PRIVATE_H

#include <stdint.h>
#include <dc/matrix.h>
#include <dc/pvr.h>
#include <dc/vec3f.h>
#include <dc/fmath.h>
#include <dc/matrix3d.h>

#include "../include/gl.h"
#include "../containers/aligned_vector.h"
#include "../containers/named_array.h"
#include "sh4_math.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* memcpy4 (void *dest, const void *src, size_t count);

#ifdef __cplusplus
#define GL_FORCE_INLINE inline
#else
#define GL_NO_INSTRUMENT inline __attribute__((no_instrument_function))
#define GL_INLINE_DEBUG GL_NO_INSTRUMENT __attribute__((always_inline))
#define GL_FORCE_INLINE static GL_INLINE_DEBUG
#endif

#define _GL_UNUSED(x) (void)(x)

#define FASTCPY(dst, src, bytes) \
    (bytes % 32 == 0) ? sq_cpy(dst, src, bytes) : memcpy(dst, src, bytes);

#define FASTCPY4(dst, src, bytes) \
    (bytes % 32 == 0) ? sq_cpy(dst, src, bytes) : memcpy4(dst, src, bytes);

#define _PACK4(v) ((v * 0xF) / 0xFF)
#define PACK_ARGB4444(a,r,g,b) (_PACK4(a) << 12) | (_PACK4(r) << 8) | (_PACK4(g) << 4) | (_PACK4(b))
#define PACK_ARGB8888(a,r,g,b) ( ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF) )
#define PACK_ARGB1555(a,r,g,b) \
    (((GLushort)(a > 0) << 15) | (((GLushort) r >> 3) << 10) | (((GLushort)g >> 3) << 5) | ((GLushort)b >> 3))

#define PACK_RGB565(r,g,b) \
    ((((GLushort)r & 0xf8) << 8) | (((GLushort) g & 0xfc) << 3) | ((GLushort) b >> 3))

#define TRACE_ENABLED 0
#define TRACE() if(TRACE_ENABLED) {fprintf(stderr, "%s\n", __func__);} (void) 0

#define VERTEX_ENABLED_FLAG     (1 << 0)
#define UV_ENABLED_FLAG         (1 << 1)
#define ST_ENABLED_FLAG         (1 << 2)
#define DIFFUSE_ENABLED_FLAG    (1 << 3)
#define NORMAL_ENABLED_FLAG     (1 << 4)

#define MAX_TEXTURE_SIZE 1024

typedef float Matrix4x4[16];

/* This gives us an easy way to switch
 * internal matrix order if necessary */

#define TRANSPOSE 0

#if TRANSPOSE
#define M0 0
#define M1 4
#define M2 8
#define M3 12
#define M4 1
#define M5 5
#define M6 9
#define M7 13
#define M8 2
#define M9 6
#define M10 10
#define M11 14
#define M12 3
#define M13 7
#define M14 11
#define M15 15
#else
#define M0 0
#define M1 1
#define M2 2
#define M3 3
#define M4 4
#define M5 5
#define M6 6
#define M7 7
#define M8 8
#define M9 9
#define M10 10
#define M11 11
#define M12 12
#define M13 13
#define M14 14
#define M15 15
#endif

typedef struct {
    pvr_poly_hdr_t hdr;
} PVRHeader;

typedef struct {
    unsigned int flags;      /* Constant PVR_CMD_USERCLIP */
    unsigned int d1, d2, d3; /* Ignored for this type */
    unsigned int sx,         /* Start x */
             sy,         /* Start y */
             ex,         /* End x */
             ey;         /* End y */
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
    //0
    GLuint   index;
    GLuint   color; /* This is the PVR texture format */
    //8
    GLenum minFilter;
    GLenum magFilter;
    //16
    GLvoid *data;
    TexturePalette* palette;
    //24
    GLushort width;
    GLushort height;
    //28
    GLushort  mipmap;  /* Bitmask of supplied mipmap levels */
    /* When using the shared palette, this is the bank (0-3) */
    GLushort shared_bank;
    //32
    GLuint dataStride;
    //36
    GLubyte mipmap_bias;
    GLubyte  env;
    GLubyte mipmapCount; /* The number of mipmap levels */
    GLubyte  uv_clamp;
    //40
    /* Mipmap textures have a different
     * offset for the base level when supplying the data, this
     * keeps track of that. baseDataOffset == 0
     * means that the texture has no mipmaps
     */
    GLuint baseDataOffset;
    GLuint baseDataSize; /* The data size of mipmap level 0 */
    //48
    GLboolean isCompressed;
    GLboolean isPaletted;
    //50
} TextureObject;

typedef struct {
    GLfloat emissive[4];
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];

    /* Valid values are 0-128 */
    GLfloat exponent;

    /* Base ambient + emission colour for
     * the current material + light */
    GLfloat baseColour[4];
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

    GLboolean isDirectional;
    GLboolean isEnabled;

    /* We set these when the material changes
     * so we don't calculate them per-vertex. They are
     * light_value * materia_value */
    GLfloat ambientMaterial[4];
    GLfloat diffuseMaterial[4];
    GLfloat specularMaterial[4];
} LightSource;

#define DEAD 0xDEADBEEF

typedef struct {
    /* Same 32 byte layout as pvr_vertex_t */
    uint32_t flags;
    float xyz[3];
    float uv[2];
    uint8_t bgra[4];
    uint8_t obgra[4];
    /* End 32 pvr_vertex_t */

    /*
     * The following are necessary for our purposes
     * W - W coordinate - for clipping
     * ST - ST coordinate for multitexture
     * NXYZ - Normal
     */

    float w; // 4
    float st[2]; // +8 (12)
    float nxyz[3]; // +12 (24)
    uint8_t visible; // +1 (25)

    uint8_t padding0[3]; // +3 (28)
    uint32_t padding1; // +4 (32)
} Vertex;


#define argbcpy(dst, src) \
    *((GLuint*) dst) = *((GLuint*) src) \


typedef struct {
    float xy[2];
} _glvec2;

typedef struct {
    float xyz[3];
} _glvec3;

typedef struct {
    float xyzw[4];
} _glvec4;

#define vec2cpy(dst, src) \
    *((_glvec2*) dst) = *((_glvec2*) src)

#define vec3cpy(dst, src) \
    *((_glvec3*) dst) = *((_glvec3*) src)

#define vec4cpy(dst, src) \
    *((_glvec4*) dst) = *((_glvec4*) src)

GL_FORCE_INLINE float clamp(float d, float min, float max) {
    return (d < min) ? min : (d > max) ? max : d;
}

#define swapVertex(a, b)   \
do {                 \
    Vertex c = *a;   \
    *a = *b;         \
    *b = c;          \
} while(0)

/* Generating PVR vertices from the user-submitted data gets complicated, particularly
 * when a realloc could invalidate pointers. This structure holds all the information
 * we need on the target vertex array to allow passing around to the various stages (e.g. generate/clip etc.)
 */
typedef struct {
    PolyList* output;
    uint32_t header_offset; // The offset of the header in the output list
    uint32_t start_offset; // The offset into the output list
    uint32_t count; // The number of vertices in this output

    /* Pointer to count * VertexExtra; */
    AlignedVector* extras;
} SubmissionTarget;

PVRHeader* _glSubmissionTargetHeader(SubmissionTarget* target);
Vertex* _glSubmissionTargetStart(SubmissionTarget* target);
Vertex* _glSubmissionTargetEnd(SubmissionTarget* target);

typedef enum {
    CLIP_RESULT_ALL_IN_FRONT,
    CLIP_RESULT_ALL_BEHIND,
    CLIP_RESULT_ALL_ON_PLANE,
    CLIP_RESULT_FRONT_TO_BACK,
    CLIP_RESULT_BACK_TO_FRONT
} ClipResult;


#define A8IDX 3
#define R8IDX 2
#define G8IDX 1
#define B8IDX 0

void _glClipTriangleStrip(SubmissionTarget* target, uint8_t fladeShade);

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

extern GLfloat DEPTH_RANGE_MULTIPLIER_L;
extern GLfloat DEPTH_RANGE_MULTIPLIER_H;

Matrix4x4* _glGetProjectionMatrix();
Matrix4x4* _glGetModelViewMatrix();

void _glWipeTextureOnFramebuffers(GLuint texture);
GLubyte _glCheckImmediateModeInactive(const char* func);

pvr_poly_cxt_t* _glGetPVRContext();
GLubyte _glInitTextures();

void _glUpdatePVRTextureContext(pvr_poly_cxt_t* context, GLshort textureUnit);
void _glAllocateSpaceForMipmaps(TextureObject* active);

typedef struct {
    const void* ptr;
    GLenum type;
    GLsizei stride;
    GLint size;
} AttribPointer;

GLboolean _glCheckValidEnum(GLint param, GLint* values, const char* func);

GLuint* _glGetEnabledAttributes();
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
GLubyte* _glGetMipmapLocation(const TextureObject* obj, GLuint level);
GLuint _glGetMipmapLevelCount(const TextureObject* obj);

GLboolean _glIsLightingEnabled();
void _glEnableLight(GLubyte light, unsigned char value);
GLboolean _glIsColorMaterialEnabled();

GLboolean _glIsNormalizeEnabled();

GLboolean _glRecalcFastPath();

typedef struct {
    float xyz[3]; // 12 bytes
    float n[3]; // 12 bytes
    float finalColour[4]; //16 bytes (to 40)
} EyeSpaceData;

extern void _glPerformLighting(Vertex* vertices, EyeSpaceData *es, const int32_t count);

unsigned char _glIsClippingEnabled();
void _glEnableClipping(unsigned char v);

void _glKosThrowError(GLenum error, const char *function);
void _glKosPrintError();
GLubyte _glKosHasError();

#define PVR_VERTEX_BUF_SIZE 2560 * 256
#define MAX_TEXTURE_UNITS 2
#define MAX_LIGHTS 8

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP( X, _MIN, _MAX )  ( (X)<(_MIN) ? (_MIN) : ((X)>(_MAX) ? (_MAX) : (X)) )

#ifdef __cplusplus
}
#endif

#endif // PRIVATE_H
