#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../include/gl.h"
#include "../include/glext.h"
#include "private.h"

typedef struct {
    const void* ptr;
    GLenum type;
    GLsizei stride;
    GLint size;
} AttribPointer;


static AttribPointer VERTEX_POINTER;
static AttribPointer UV_POINTER;
static AttribPointer ST_POINTER;
static AttribPointer NORMAL_POINTER;
static AttribPointer DIFFUSE_POINTER;

#define VERTEX_ENABLED_FLAG     (1 << 0)
#define UV_ENABLED_FLAG         (1 << 1)
#define ST_ENABLED_FLAG         (1 << 2)
#define DIFFUSE_ENABLED_FLAG    (1 << 3)
#define NORMAL_ENABLED_FLAG     (1 << 4)

static GLuint ENABLED_VERTEX_ATTRIBUTES = 0;
static GLubyte ACTIVE_CLIENT_TEXTURE = 0;

void initAttributePointers() {
    TRACE();

    VERTEX_POINTER.ptr = NULL;
    VERTEX_POINTER.stride = 0;
    VERTEX_POINTER.type = GL_FLOAT;
    VERTEX_POINTER.size = 4;

    DIFFUSE_POINTER.ptr = NULL;
    DIFFUSE_POINTER.stride = 0;
    DIFFUSE_POINTER.type = GL_FLOAT;
    DIFFUSE_POINTER.size = 4;

    UV_POINTER.ptr = NULL;
    UV_POINTER.stride = 0;
    UV_POINTER.type = GL_FLOAT;
    UV_POINTER.size = 4;

    ST_POINTER.ptr = NULL;
    ST_POINTER.stride = 0;
    ST_POINTER.type = GL_FLOAT;
    ST_POINTER.size = 4;

    NORMAL_POINTER.ptr = NULL;
    NORMAL_POINTER.stride = 0;
    NORMAL_POINTER.type = GL_FLOAT;
    NORMAL_POINTER.size = 3;
}

static GLuint byte_size(GLenum type) {
    switch(type) {
    case GL_BYTE: return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_SHORT: return sizeof(GLshort);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    case GL_INT: return sizeof(GLint);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    case GL_DOUBLE: return sizeof(GLdouble);
    case GL_FLOAT:
    default: return sizeof(GLfloat);
    }
}

static void _parseColour(uint32* out, const GLubyte* in, GLint size, GLenum type) {
    switch(type) {
    case GL_BYTE: {
    case GL_UNSIGNED_BYTE:
        *out = in[3] << 24 | in[0] << 16 | in[1] << 8 | in[0];
    } break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
        /* FIXME!!!! */
    break;
    case GL_INT:
    case GL_UNSIGNED_INT:
        /* FIXME!!!! */
    break;
    case GL_FLOAT:
    case GL_DOUBLE:
    default: {
        const GLfloat* src = (GLfloat*) in;
        *out = PVR_PACK_COLOR(src[3], src[0], src[1], src[2]);
    } break;
    }
}

static void _parseFloats(GLfloat* out, const GLubyte* in, GLint size, GLenum type) {
    GLubyte i;

    switch(type) {
    case GL_SHORT: {
        GLshort* inp = (GLshort*) in;
        for(i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_INT: {
        GLint* inp = (GLint*) in;
        for(i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_FLOAT:
    case GL_DOUBLE:  /* Double == Float */
        default: {
            const GLfloat* ptr = (const GLfloat*) in;
            for(i = 0; i < size; ++i) out[i] = ptr[i];
        }
    }
}

static void _parseIndex(GLshort* out, const GLubyte* in, GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE:
        *out = (GLshort) *in;
    break;
    case GL_UNSIGNED_SHORT:
    default:
        *out = *((GLshort*) in);
    }
}


/* There was a bug in this macro that shipped with Kos
 * which has now been fixed. But just in case...
 */
#undef mat_trans_single3_nodiv
#define mat_trans_single3_nodiv(x, y, z) { \
    register float __x __asm__("fr12") = (x); \
    register float __y __asm__("fr13") = (y); \
    register float __z __asm__("fr14") = (z); \
    __asm__ __volatile__( \
                          "fldi1 fr15\n" \
                          "ftrv  xmtrx, fv12\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr15"); \
    x = __x; y = __y; z = __z; \
}

#undef mat_trans_normal3
#define mat_trans_normal3(x, y, z) { \
    register float __x __asm__("fr8") = (x); \
    register float __y __asm__("fr9") = (y); \
    register float __z __asm__("fr10") = (z); \
    __asm__ __volatile__( \
                          "fldi0 fr11\n" \
                          "ftrv  xmtrx, fv8\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr11"); \
    x = __x; y = __y; z = __z; \
}



inline void transformToEyeSpace(GLfloat* point) {
    _matrixLoadModelView();
    mat_trans_single3_nodiv(point[0], point[1], point[2]);
}


inline void transformNormalToEyeSpace(GLfloat* normal) {
    _matrixLoadNormal();
    mat_trans_normal3(normal[0], normal[1], normal[2]);
}


typedef struct {
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Colour;

/* Note: This structure is the almost the same format as pvr_vertex_t aside from the offet
 * (oargb) which is replaced by the floating point w value. This is so that we can
 * simply zero it and memcpy the lot into the output */
typedef struct {
    uint32_t flags;
    float xyz[3];
    float uv[2];
    Colour argb;
    float nxyz[3];
    float w;

    float xyzES[3]; /* Coordinate in eye space */
    float nES[3]; /* Normal in eye space */
} ClipVertex;


static void swapVertex(ClipVertex* v1, ClipVertex* v2) {
    ClipVertex tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

static void generate(AlignedVector* output, const GLenum mode, const GLsizei first, const GLsizei count,
        const GLubyte* indices, const GLenum type,
        const GLubyte* vptr, const GLubyte vstride, const GLubyte* cptr, const GLubyte cstride,
        const GLubyte* uvptr, const GLubyte uvstride, const GLubyte* nptr, const GLubyte nstride) {
    /* Read from the client buffers and generate an array of ClipVertices */

    GLsizei max = first + count;

    GLsizei spaceNeeded = (mode == GL_POLYGON || mode == GL_TRIANGLE_FAN) ? ((count - 2) * 3) : count;

    /* Make sure we have room for the output */
    aligned_vector_resize(output, spaceNeeded);

    ClipVertex* vertex = (ClipVertex*) output->data;

    GLsizei j;
    GLsizei i = 0;
    for(j = first; j < max; ++i, ++j, ++vertex) {
        vertex->flags = PVR_CMD_VERTEX;

        GLshort idx = j;
        if(indices) {
            _parseIndex(&idx, &indices[byte_size(type) * j], type);
        }

        _parseFloats(vertex->xyz, vptr + (idx * vstride), VERTEX_POINTER.size, VERTEX_POINTER.type);

        if(ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) {
            _parseColour((uint32_t*) &vertex->argb, cptr + (idx * cstride), DIFFUSE_POINTER.size, DIFFUSE_POINTER.type);
        } else {
            /* Default to white if colours are disabled */
            vertex->argb.a = 255;
            vertex->argb.r = 255;
            vertex->argb.g = 255;
            vertex->argb.b = 255;
        }

        if(ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) {
            _parseFloats(vertex->uv, uvptr + (idx * uvstride), UV_POINTER.size, UV_POINTER.type);
        }

        if(ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) {
            _parseFloats(vertex->nxyz, nptr + (idx * nstride), NORMAL_POINTER.size, NORMAL_POINTER.type);
        } else {
            vertex->nxyz[0] = 0.0f;
            vertex->nxyz[1] = 0.0f;
            vertex->nxyz[2] = -1.0f;
        }

        if((mode == GL_TRIANGLES) && ((i + 1) % 3) == 0) {
            vertex->flags = PVR_CMD_VERTEX_EOL;
        } else if((mode == GL_QUADS) && ((i + 1) % 4) == 0) {
            ClipVertex* previous = vertex - 1;
            previous->flags = PVR_CMD_VERTEX_EOL;
            swapVertex(previous, vertex);
        } else if((mode == GL_POLYGON || mode == GL_TRIANGLE_FAN)) {
            ClipVertex* previous = vertex - 1;
            if(i == 2) {
                swapVertex(previous, vertex);
                vertex->flags = PVR_CMD_VERTEX_EOL;
            } else if(i > 2) {
                ClipVertex* first = (ClipVertex*) output->data;
                ClipVertex* previous = vertex - 1;
                ClipVertex* next = vertex + 1;

                *next = *first;

                swapVertex(next, vertex);

                vertex = next + 1;
                *vertex = *previous;

                vertex->flags = PVR_CMD_VERTEX_EOL;
            }
        } else if(mode == GL_TRIANGLE_STRIP && j == (max - 1)) {
            /* If the mode was triangle strip, then the last vertex is the last vertex */
            vertex->flags = PVR_CMD_VERTEX_EOL;
        }
    }
}

static void transform(AlignedVector* vertices) {
    /* Perform modelview transform, storing W */

    ClipVertex* vertex = (ClipVertex*) vertices->data;

    _applyRenderMatrix(); /* Apply the Render Matrix Stack */

    GLsizei i;
    for(i = 0; i < vertices->size; ++i, ++vertex) {
        register float __x __asm__("fr12") = (vertex->xyz[0]);
        register float __y __asm__("fr13") = (vertex->xyz[1]);
        register float __z __asm__("fr14") = (vertex->xyz[2]);
        register float __w __asm__("fr15") = 1.0f;

        __asm__ __volatile__(
            "ftrv   xmtrx,fv12\n"
            : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w)
            : "0" (__x), "1" (__y), "2" (__z), "3" (__w)
        );

        vertex->xyz[0] = __x;
        vertex->xyz[1] = __y;
        vertex->xyz[2] = __z;
        vertex->w = __w;
    }
}

static void clip(AlignedVector* vertices) {
    /* Perform clipping, generating new vertices as necessary */
}

static void mat_transform3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t stride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_single3_nodiv_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += stride;
        dataOut += stride;
    }
}

static void mat_transform_normal3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t stride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += stride;
        dataOut += stride;
    }
}

static void light(AlignedVector* vertices) {
    if(!isLightingEnabled()) {
        return;
    }

    /* Perform lighting calculations and manipulate the colour */
    ClipVertex* vertex = (ClipVertex*) vertices->data;

    _matrixLoadModelView();
    mat_transform3(vertex->xyz, vertex->xyzES, vertices->size, sizeof(ClipVertex));

    _matrixLoadNormal();
    mat_transform_normal3(vertex->nxyz, vertex->nES, vertices->size, sizeof(ClipVertex));

    GLsizei i;
    for(i = 0; i < vertices->size; ++i, ++vertex) {
        /* We ignore diffuse colour when lighting is enabled. If GL_COLOR_MATERIAL is enabled
         * then the lighting calculation should possibly take it into account */
        GLfloat contribution [] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLfloat to_add [] = {0.0f, 0.0f, 0.0f, 0.0f};

        GLubyte j;
        for(j = 0; j < MAX_LIGHTS; ++j) {
            if(isLightEnabled(j)) {
                calculateLightingContribution(j, vertex->xyzES, vertex->nES, to_add);

                contribution[0] += to_add[0];
                contribution[1] += to_add[1];
                contribution[2] += to_add[2];
                contribution[3] += to_add[3];
            }
        }

        uint32_t final = PVR_PACK_COLOR(contribution[3], contribution[0], contribution[1], contribution[2]);
        vertex->argb = *((Colour*) &final);
    }
}

static void divide(AlignedVector* vertices) {
    /* Perform perspective divide on each vertex */
    ClipVertex* vertex = (ClipVertex*) vertices->data;

    GLsizei i;
    for(i = 0; i < vertices->size; ++i, ++vertex) {
        vertex->xyz[2] = 1.0f / vertex->w;
        vertex->xyz[0] *= vertex->xyz[2];
        vertex->xyz[1] *= vertex->xyz[2];
    }
}

static void push(const AlignedVector* vertices, PolyList* activePolyList) {
    /* Copy the vertices to the active poly list */

    // Make room for the element + the header
    PVRCommand* dst = (PVRCommand*) aligned_vector_extend(&activePolyList->vector, vertices->size + 1);

    // Store a pointer to the header
    pvr_poly_hdr_t* hdr = (pvr_poly_hdr_t*) dst;

    // Point dest at the first new vertex to populate
    dst++;

    // Compile
    pvr_poly_cxt_t cxt = *getPVRContext();
    cxt.list_type = activePolyList->list_type;

    updatePVRTextureContext(&cxt, getTexture0());

    pvr_poly_compile(hdr, &cxt);

    GLsizei i;
    for(i = 0; i < vertices->size; ++i, dst++) {
        pvr_vertex_t* vout = (pvr_vertex_t*) dst;

        /* The first part of ClipVertex is the same as the first part of pvr_vertex_t */
        memcpy(vout, aligned_vector_at(vertices, i), sizeof(pvr_vertex_t));

        /* Except for this bit */
        vout->oargb = 0;
    }
}

static void submitVertices(GLenum mode, GLsizei first, GLsizei count, GLenum type, const GLvoid* indices) {
    static AlignedVector* buffer = NULL;

    /* Do nothing if vertices aren't enabled */
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    /* Initialize the buffer on first call */
    if(!buffer) {
        buffer = (AlignedVector*) malloc(sizeof(AlignedVector));
        aligned_vector_init(buffer, sizeof(ClipVertex));
    } else {
        /* Else, resize to zero (this will retain the allocated memory) */
        aligned_vector_resize(buffer, 0);
    }

    GLubyte vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    const GLubyte* vptr = VERTEX_POINTER.ptr;

    GLubyte cstride = (DIFFUSE_POINTER.stride) ? DIFFUSE_POINTER.stride : DIFFUSE_POINTER.size * byte_size(DIFFUSE_POINTER.type);
    const GLubyte* cptr = DIFFUSE_POINTER.ptr;

    GLubyte uvstride = (UV_POINTER.stride) ? UV_POINTER.stride : UV_POINTER.size * byte_size(UV_POINTER.type);
    const GLubyte* uvptr = UV_POINTER.ptr;

    GLubyte nstride = (NORMAL_POINTER.stride) ? NORMAL_POINTER.stride : NORMAL_POINTER.size * byte_size(NORMAL_POINTER.type);
    const GLubyte* nptr = NORMAL_POINTER.ptr;

    generate(buffer, mode, first, count, (GLubyte*) indices, type, vptr, vstride, cptr, cstride, uvptr, uvstride, nptr, nstride);
    light(buffer);
    transform(buffer);
    divide(buffer);

    push(buffer, activePolyList());
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(checkImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(checkImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, first, count, GL_UNSIGNED_SHORT, NULL);
}

void APIENTRY glEnableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES |= ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glEnableClientState");
    }
}

void APIENTRY glDisableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES &= ~ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES &= ~UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glDisableClientState");
    }
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, "glClientActiveTextureARB");
    }

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}

void APIENTRY glTexCoordPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &UV_POINTER : &ST_POINTER;

    tointer->ptr = pointer;
    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;
}

void APIENTRY glVertexPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    VERTEX_POINTER.ptr = pointer;
    VERTEX_POINTER.stride = stride;
    VERTEX_POINTER.type = type;
    VERTEX_POINTER.size = size;
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    DIFFUSE_POINTER.ptr = pointer;
    DIFFUSE_POINTER.stride = stride;
    DIFFUSE_POINTER.type = type;
    DIFFUSE_POINTER.size = size;
}

void APIENTRY glNormalPointer(GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    NORMAL_POINTER.ptr = pointer;
    NORMAL_POINTER.stride = stride;
    NORMAL_POINTER.type = type;
    NORMAL_POINTER.size = 3;
}
