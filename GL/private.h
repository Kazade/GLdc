#ifndef PRIVATE_H
#define PRIVATE_H

#include <stdint.h>
#include <stdio.h>

#include "gl_assert.h"
#include "platform.h"
#include "types.h"

#include "../include/GL/gl.h"
#include "../include/GL/glext.h"
#include "../include/GL/glkos.h"

#include "../containers/aligned_vector.h"
#include "../containers/named_array.h"

#define MAX_GLDC_4BPP_PALETTE_SLOTS 16
#define MAX_GLDC_PALETTE_SLOTS 4
#define MAX_GLDC_SHARED_PALETTES (MAX_GLDC_PALETTE_SLOTS*MAX_GLDC_4BPP_PALETTE_SLOTS)


extern void* memcpy4 (void *dest, const void *src, size_t count);

#define GL_NO_INSTRUMENT inline __attribute__((no_instrument_function))
#define GL_INLINE_DEBUG GL_NO_INSTRUMENT __attribute__((always_inline))
#define GL_FORCE_INLINE static GL_INLINE_DEBUG
#define GL_NO_INLINE __attribute__((noinline))
#define _GL_UNUSED(x) (void)(x)

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
    GLenum internalFormat;
    //54
    GLubyte padding[10];  // Pad to 64-bytes
} __attribute__((aligned(32))) TextureObject;

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


#define argbcpy(dst, src) \
    *((GLuint*) dst) = *((const GLuint*) src) \


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

GL_FORCE_INLINE void memcpy_vertex(Vertex *dest, const Vertex *src) {
#ifdef __DREAMCAST__
    _Complex float double_scratch;

    asm volatile (
        "fschg\n\t"
        "clrs\n\t"
        ".align 2\n\t"
        "fmov.d @%[in]+, %[scratch]\n\t"
        "fmov.d %[scratch], @%[out]\n\t"
        "fmov.d @%[in]+, %[scratch]\n\t"
        "add #8, %[out]\n\t"
        "fmov.d %[scratch], @%[out]\n\t"
        "fmov.d @%[in]+, %[scratch]\n\t"
        "add #8, %[out]\n\t"
        "fmov.d %[scratch], @%[out]\n\t"
        "fmov.d @%[in], %[scratch]\n\t"
        "add #8, %[out]\n\t"
        "fmov.d %[scratch], @%[out]\n\t"
        "fschg\n"
        : [in] "+&r" ((uint32_t) src), [scratch] "=&d" (double_scratch), [out] "+&r" ((uint32_t) dest)
        :
        : "t", "memory" // clobbers
    );
#else
    *dest = *src;
#endif
}

#define swapVertex(a, b)   \
do {                 \
    Vertex __attribute__((aligned(32))) c;   \
    memcpy_vertex(&c, a); \
    memcpy_vertex(a, b); \
    memcpy_vertex(b, &c); \
} while(0)

#ifdef __DREAMCAST__
#define fast_rsqrt(x) frsqrt(x)
#else
#define fast_rsqrt(x) (1.0f / __builtin_sqrtf(x))
#endif

/* ClipVertex doesn't have room for these, so we need to parse them
 * out separately. Potentially 'w' will be housed here if we support oargb */
typedef struct {
    float nxyz[3];
    float st[2];
} VertexExtra;

/* Generating PVR vertices from the user-submitted data gets complicated, particularly
 * when a realloc could invalidate pointers. This structure holds all the information
 * we need on the target vertex array to allow passing around to the various stages (e.g. generate/clip etc.)
 */
typedef struct __attribute__((aligned(32))) {
    PolyList* output;
    uint32_t header_offset; // The offset of the header in the output list
    uint32_t start_offset; // The offset into the output list
    uint32_t count; // The number of vertices in this output

    /* Pointer to count * VertexExtra; */
    AlignedVector* extras;
} SubmissionTarget;

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

struct SubmissionTarget;

PolyList* _glOpaquePolyList();
PolyList* _glPunchThruPolyList();
PolyList *_glTransparentPolyList();

void _glInitAttributePointers();
void _glInitContext();
void _glInitLights();
void _glInitImmediateMode(GLuint initial_size);
void _glInitMatrices();
void _glInitFramebuffers();
void _glInitSubmissionTarget();

void _glMatrixLoadNormal();
void _glMatrixLoadModelView();
void _glMatrixLoadProjection();
void _glMatrixLoadTexture();
void _glMatrixLoadModelViewProjection();

extern GLfloat DEPTH_RANGE_MULTIPLIER_L;
extern GLfloat DEPTH_RANGE_MULTIPLIER_H;

extern GLfloat HALF_LINE_WIDTH;
extern GLfloat HALF_POINT_SIZE;

Matrix4x4* _glGetProjectionMatrix();
Matrix4x4* _glGetModelViewMatrix();

void _glWipeTextureOnFramebuffers(GLuint texture);

GLubyte _glInitTextures();

void _glUpdatePVRTextureContext(PolyContext* context, GLshort textureUnit);
void _glAllocateSpaceForMipmaps(TextureObject* active);

typedef struct {
    const void* ptr;  // 4
    GLenum type;  // 4
    GLsizei stride;  // 4
    GLint size; // 4
} AttribPointer;

typedef struct {
    AttribPointer vertex; // 16
    AttribPointer colour; // 32
    AttribPointer uv; // 48
    AttribPointer st; // 64
    AttribPointer normal; // 80
    AttribPointer padding; // 96
} AttribPointerList;

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

extern GLubyte ACTIVE_TEXTURE;
extern GLboolean TEXTURES_ENABLED[];

GLubyte _glGetActiveTexture();
GLint _glGetTextureInternalFormat();
GLboolean _glGetTextureTwiddle();
void _glSetTextureTwiddle(GLboolean v);

GLuint _glGetActiveClientTexture();
TexturePalette* _glGetSharedPalette(GLshort bank);
void _glSetInternalPaletteFormat(GLenum val);

GLboolean _glIsSharedTexturePaletteEnabled();
void _glApplyColorTable(TexturePalette *palette);

GLboolean _glIsBlendingEnabled();
GLboolean _glIsAlphaTestEnabled();
GLboolean _glIsCullingEnabled();
GLboolean _glIsDepthTestEnabled();
GLboolean _glIsDepthWriteEnabled();
GLboolean _glIsScissorTestEnabled();
GLboolean _glIsFogEnabled();
GLenum _glGetDepthFunc();
GLenum _glGetCullFace();
GLenum _glGetFrontFace();
GLenum _glGetGpuBlendSrcFactor();
GLenum _glGetGpuBlendDstFactor();

extern PolyList OP_LIST;
extern PolyList PT_LIST;
extern PolyList TR_LIST;

GL_FORCE_INLINE PolyList* _glActivePolyList() {
    if(_glIsBlendingEnabled()) {
        return &TR_LIST;
    } else if(_glIsAlphaTestEnabled()) {
        return &PT_LIST;
    } else {
        return &OP_LIST;
    }
}

GLboolean _glIsMipmapComplete(const TextureObject* obj);
GLubyte* _glGetMipmapLocation(const TextureObject* obj, GLuint level);
GLuint _glGetMipmapLevelCount(const TextureObject* obj);
GLboolean _glIsLightingEnabled();

void _glEnableLight(GLubyte light, GLboolean value);
GLboolean _glIsColorMaterialEnabled();

GLboolean _glIsNormalizeEnabled();

extern AttribPointerList ATTRIB_POINTERS;

extern GLuint ENABLED_VERTEX_ATTRIBUTES;
extern GLuint FAST_PATH_ENABLED;

GL_FORCE_INLINE GLuint _glIsVertexDataFastPathCompatible() {
    /* The fast path is enabled when all enabled elements of the vertex
     * match the output format. This means:
     *
     * xyz == 3f
     * uv == 2f
     * rgba == argb4444
     * st == 2f
     * normal == 3f
     *
     * When this happens we do inline straight copies of the enabled data
     * and transforms for positions and normals happen while copying.
     */



    if((ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        if(ATTRIB_POINTERS.vertex.size != 3 || ATTRIB_POINTERS.vertex.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG)) {
        if(ATTRIB_POINTERS.uv.size != 2 || ATTRIB_POINTERS.uv.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG)) {
        /* FIXME: Shouldn't this be a reversed format? */
        if(ATTRIB_POINTERS.colour.size != GL_BGRA || ATTRIB_POINTERS.colour.type != GL_UNSIGNED_BYTE) {
            return GL_FALSE;
        }
    }

    if((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG)) {
        if(ATTRIB_POINTERS.st.size != 2 || ATTRIB_POINTERS.st.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG)) {
        if(ATTRIB_POINTERS.normal.size != 3 || ATTRIB_POINTERS.normal.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    return GL_TRUE;
}

GL_FORCE_INLINE GLuint _glRecalcFastPath() {
    FAST_PATH_ENABLED = _glIsVertexDataFastPathCompatible();
    return FAST_PATH_ENABLED;
}

extern GLboolean IMMEDIATE_MODE_ACTIVE;

extern GLenum LAST_ERROR;
extern char ERROR_FUNCTION[64];

GL_FORCE_INLINE const char* _glErrorEnumAsString(GLenum error) {
    switch(error) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        default:
            return "GL_UNKNOWN_ERROR";
    }
}

GL_FORCE_INLINE void _glKosThrowError(GLenum error, const char *function) {
    if(LAST_ERROR == GL_NO_ERROR) {
        LAST_ERROR = error;
        sprintf(ERROR_FUNCTION, "%s\n", function);
        fprintf(stderr, "GL ERROR: %s when calling %s\n", _glErrorEnumAsString(LAST_ERROR), ERROR_FUNCTION);
    }
}

GL_FORCE_INLINE GLubyte _glKosHasError() {
    return (LAST_ERROR != GL_NO_ERROR) ? GL_TRUE : GL_FALSE;
}

GL_FORCE_INLINE void _glKosResetError() {
    LAST_ERROR = GL_NO_ERROR;
    sprintf(ERROR_FUNCTION, "\n");
}

GL_FORCE_INLINE GLboolean _glCheckImmediateModeInactive(const char* func) {
    /* Returns 1 on error */
    if(IMMEDIATE_MODE_ACTIVE) {
        _glKosThrowError(GL_INVALID_OPERATION, func);
        return GL_TRUE;
    }

    return GL_FALSE;
}

typedef struct {
    float n[3]; // 12 bytes
    float finalColour[4]; //28 bytes
    uint32_t padding; // 32 bytes
} EyeSpaceData;

extern void _glPerformLighting(Vertex* vertices, EyeSpaceData *es, const uint32_t count);

unsigned char _glIsClippingEnabled();
void _glEnableClipping(unsigned char v);

GLuint _glFreeTextureMemory();
GLuint _glUsedTextureMemory();
GLuint _glFreeContiguousTextureMemory();

void _glApplyScissor(bool force);
void _glSetColorMaterialMask(GLenum mask);
void _glSetColorMaterialMode(GLenum mode);
GLenum _glColorMaterialMode();

Material* _glActiveMaterial();
void _glSetLightModelViewerInEyeCoordinates(GLboolean v);
void _glSetLightModelSceneAmbient(const GLfloat* v);
void _glSetLightModelColorControl(GLint v);
GLuint _glEnabledLightCount();
void _glRecalcEnabledLights();
GLfloat* _glLightModelSceneAmbient();
GLfloat* _glGetLightModelSceneAmbient();
LightSource* _glLightAt(GLuint i);
GLboolean _glNearZClippingEnabled();

GLboolean _glGPUStateIsDirty();
void _glGPUStateMarkClean();
void _glGPUStateMarkDirty();

#define MAX_GLDC_TEXTURE_UNITS 2
#define MAX_GLDC_LIGHTS 8

#define AMBIENT_MASK 1
#define DIFFUSE_MASK 2
#define EMISSION_MASK 4
#define SPECULAR_MASK 8
#define SCENE_AMBIENT_MASK 16


/* This is from KOS pvr_buffers.c */
#define PVR_MIN_Z 0.0001f

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP( X, _MIN, _MAX )  ( (X)<(_MIN) ? (_MIN) : ((X)>(_MAX) ? (_MAX) : (X)) )

#endif // PRIVATE_H
