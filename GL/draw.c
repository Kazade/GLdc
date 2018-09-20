#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../include/gl.h"
#include "../include/glext.h"
#include "private.h"
#include "profiler.h"


static AttribPointer VERTEX_POINTER;
static AttribPointer UV_POINTER;
static AttribPointer ST_POINTER;
static AttribPointer NORMAL_POINTER;
static AttribPointer DIFFUSE_POINTER;

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

static inline GLuint byte_size(GLenum type) {
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

typedef void (*FloatParseFunc)(GLfloat* out, const GLubyte* in);
typedef void (*ByteParseFunc)(GLubyte* out, const GLubyte* in);
typedef void (*PolyBuildFunc)(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i);

GLuint _glGetEnabledAttributes() {
    return ENABLED_VERTEX_ATTRIBUTES;
}

AttribPointer* _glGetVertexAttribPointer() {
    return &VERTEX_POINTER;
}

AttribPointer* _glGetDiffuseAttribPointer() {
    return &DIFFUSE_POINTER;
}

AttribPointer* _glGetNormalAttribPointer() {
    return &NORMAL_POINTER;
}

AttribPointer* _glGetUVAttribPointer() {
    return &UV_POINTER;
}

AttribPointer* _glGetSTAttribPointer() {
    return &ST_POINTER;
}

static inline void _parseVec3FromShort3(GLfloat* out, const GLubyte* in) {
    GLshort* ptr = (GLshort*) in;

    out[0] = (GLfloat) ptr[0];
    out[1] = (GLfloat) ptr[1];
    out[2] = (GLfloat) ptr[2];
}

static inline void _parseVec3FromInt3(GLfloat* out, const GLubyte* in) {
    GLint* ptr = (GLint*) in;

    out[0] = (GLfloat) ptr[0];
    out[1] = (GLfloat) ptr[1];
    out[2] = (GLfloat) ptr[2];
}

static inline void _parseVec3FromFloat3(GLfloat* out, const GLubyte* in) {
    GLfloat* ptr = (GLfloat*) in;

    out[0] = ptr[0];
    out[1] = ptr[1];
    out[2] = ptr[2];
}

static inline void _parseVec2FromFloat2(GLfloat* out, const GLubyte* in) {
    GLfloat* ptr = (GLfloat*) in;

    out[0] = ptr[0];
    out[1] = ptr[1];
}

static inline void _parseVec3FromFloat2(GLfloat* out, const GLubyte* in) {
    GLfloat* ptr = (GLfloat*) in;

    out[0] = ptr[0];
    out[1] = ptr[1];
    out[2] = 0.0f;
}

static inline void _parseVec4FromFloat3(GLfloat* out, const GLubyte* in) {
    GLfloat* ptr = (GLfloat*) in;

    out[0] = ptr[0];
    out[1] = ptr[1];
    out[2] = ptr[2];
    out[3] = 1.0;
}

static inline void _parseVec4FromFloat4(GLfloat* out, const GLubyte* in) {
    GLfloat* ptr = (GLfloat*) in;

    out[0] = ptr[0];
    out[1] = ptr[1];
    out[2] = ptr[2];
    out[3] = ptr[3];
}

static inline void _parseColourFromUByte4(GLubyte* out, const GLubyte* in) {
    out[R8IDX] = in[0];
    out[G8IDX] = in[1];
    out[B8IDX] = in[2];
    out[A8IDX] = in[3];
}

static inline void _parseColourFromFloat4(GLubyte* out, const GLubyte* in) {
    GLfloat* fin = (GLfloat*) in;

    out[R8IDX] = (GLubyte) (fin[0] * 255.0f);
    out[G8IDX] = (GLubyte) (fin[1] * 255.0f);
    out[B8IDX] = (GLubyte) (fin[2] * 255.0f);
    out[A8IDX] = (GLubyte) (fin[3] * 255.0f);
}

static inline void _parseColourFromFloat3(GLubyte* out, const GLubyte* in) {
    out[A8IDX] = 255;
    out[R8IDX] = (GLubyte) ((GLfloat) in[0]) * 255.0f;
    out[G8IDX] = (GLubyte) ((GLfloat) in[1]) * 255.0f;
    out[B8IDX] = (GLubyte) ((GLfloat) in[2]) * 255.0f;
}

static inline void _constVec2Zero(GLfloat* out, const GLubyte* in) {
    out[0] = 0.0f;
    out[1] = 0.0f;
}

static inline void _constVec3NegZ(GLfloat* out, const GLubyte* in) {
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = -1.0f;
}

static inline void _constVec4One(GLfloat* out, const GLubyte* in) {
    out[0] = 1.0f;
    out[1] = 1.0f;
    out[2] = 1.0f;
    out[3] = 1.0f;
}

static inline void _constColourOne(GLubyte* out, const GLubyte* in) {
    out[0] = 255;
    out[1] = 255;
    out[2] = 255;
    out[3] = 255;
}


typedef GLuint (*IndexParseFunc)(const GLubyte* in);

static inline GLuint _parseUByteIndex(const GLubyte* in) {
    return (GLuint) *in;
}

static inline GLuint _parseUIntIndex(const GLubyte* in) {
    return *((GLuint*) in);
}

static inline GLuint _parseUShortIndex(const GLubyte* in) {
    return *((GLshort*) in);
}


static inline IndexParseFunc _calcParseIndexFunc(GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE:
        return &_parseUByteIndex;
    break;
    case GL_UNSIGNED_INT:
        return &_parseUIntIndex;
    break;
    case GL_UNSIGNED_SHORT:
    default:
        break;
    }

    return &_parseUShortIndex;
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


/* FIXME: Is this right? Shouldn't it be fr12->15? */
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


static inline void transformToEyeSpace(GLfloat* point) {
    _matrixLoadModelView();
    mat_trans_single3_nodiv(point[0], point[1], point[2]);
}

static inline void transformNormalToEyeSpace(GLfloat* normal) {
    _matrixLoadNormal();
    mat_trans_normal3(normal[0], normal[1], normal[2]);
}

static inline void swapVertex(ClipVertex* v1, ClipVertex* v2) {
    static ClipVertex tmp;

    tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}

static inline FloatParseFunc _calcVertexParseFunc() {
    switch(VERTEX_POINTER.type) {
    case GL_SHORT: {
        if(VERTEX_POINTER.size == 3) {
            return &_parseVec3FromShort3;
        }
    } break;
    case GL_INT: {
        if(VERTEX_POINTER.size == 3) {
            return &_parseVec3FromInt3;
        }
    } break;
    case GL_FLOAT: {
        if(VERTEX_POINTER.size == 3) {
            return &_parseVec3FromFloat3;
        } else if(VERTEX_POINTER.size == 2) {
            return &_parseVec3FromFloat2;
        }
    } break;
    default:
        break;
    }

    return NULL;
}

static inline ByteParseFunc _calcDiffuseParseFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) {
        return &_constColourOne;
    }

    switch(DIFFUSE_POINTER.type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE: {
        if(DIFFUSE_POINTER.size == 4) {
            return &_parseColourFromUByte4;
        }
    } break;
    case GL_FLOAT: {
        if(DIFFUSE_POINTER.size == 3) {
            return &_parseColourFromFloat3;
        } else if(DIFFUSE_POINTER.size == 4) {
            return &_parseColourFromFloat4;
        }
    } break;
    default:
        break;
    }

    return &_constColourOne;
}

static inline FloatParseFunc _calcUVParseFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) {
        return &_constVec2Zero;
    }

    switch(UV_POINTER.type) {
    case GL_FLOAT: {
        if(UV_POINTER.size == 2) {
            return &_parseVec2FromFloat2;
        }
    } break;
    default:
        break;
    }

    return &_constVec2Zero;
}

static inline FloatParseFunc _calcSTParseFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG) {
        return &_constVec2Zero;
    }

    switch(ST_POINTER.type) {
    case GL_FLOAT: {
        if(ST_POINTER.size == 2) {
            return &_parseVec2FromFloat2;
        }
    } break;
    default:
        break;
    }

    return &_constVec2Zero;
}

static inline FloatParseFunc _calcNormalParseFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) != NORMAL_ENABLED_FLAG) {
        return &_constVec3NegZ;
    }

    switch(NORMAL_POINTER.type) {
    case GL_SHORT: {
        if(NORMAL_POINTER.size == 3) {
            return &_parseVec3FromShort3;
        }
    } break;
    case GL_INT: {
        if(NORMAL_POINTER.size == 3) {
            return &_parseVec3FromInt3;
        }
    } break;
    case GL_FLOAT: {
        if(NORMAL_POINTER.size == 3) {
            return &_parseVec3FromFloat3;
        } else if(NORMAL_POINTER.size == 2) {
            return &_parseVec3FromFloat2;
        }
    } break;
    default:
        break;
    }

    return &_constVec3NegZ;
}


static inline void _buildTriangle(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i) {
    if(((i + 1) % 3) == 0) {
        vertex->flags = PVR_CMD_VERTEX_EOL;
    }
}

static inline GLsizei fast_mod(const GLsizei input, const GLsizei ceil) {
    return input >= ceil ? input % ceil : input;
}

static inline void _buildQuad(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i) {
    if((i + 1) % 4 == 0) {
        previous->flags = PVR_CMD_VERTEX_EOL;
        swapVertex(previous, vertex);
    }
}

static void _buildTriangleFan(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i) {
    if(i == 2) {
        swapVertex(previous, vertex);
        vertex->flags = PVR_CMD_VERTEX_EOL;
    } else if(i > 2) {
        ClipVertex* next = vertex + 1;

        *next = *first;

        swapVertex(next, vertex);

        vertex = next + 1;
        *vertex = *previous;

        vertex->flags = PVR_CMD_VERTEX_EOL;
    }
}

static void _buildStrip(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i) {
    if(!next) {
        /* If the mode was triangle strip, then the last vertex is the last vertex */
        vertex->flags = PVR_CMD_VERTEX_EOL;
    }
}

static inline PolyBuildFunc _calcBuildFunc(const GLenum type) {
    switch(type) {
    case GL_TRIANGLES:
        return &_buildTriangle;
    break;
    case GL_QUADS:
        return &_buildQuad;
    break;
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        return &_buildTriangleFan;
    break;
    default:
        break;
    }

    return &_buildStrip;
}

static void generate(ClipVertex* output, const GLenum mode, const GLsizei first, const GLsizei count,
        const GLubyte* indices, const GLenum type, const GLboolean doTexture, const GLboolean doMultitexture, const GLboolean doLighting) {
    /* Read from the client buffers and generate an array of ClipVertices */

    const GLuint vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    const GLuint cstride = (DIFFUSE_POINTER.stride) ? DIFFUSE_POINTER.stride : DIFFUSE_POINTER.size * byte_size(DIFFUSE_POINTER.type);
    const GLuint uvstride = (UV_POINTER.stride) ? UV_POINTER.stride : UV_POINTER.size * byte_size(UV_POINTER.type);
    const GLuint ststride = (ST_POINTER.stride) ? ST_POINTER.stride : ST_POINTER.size * byte_size(ST_POINTER.type);
    const GLuint nstride = (NORMAL_POINTER.stride) ? NORMAL_POINTER.stride : NORMAL_POINTER.size * byte_size(NORMAL_POINTER.type);

    const GLsizei max = first + count;

    ClipVertex* vertex = output;

    const FloatParseFunc vertexFunc = _calcVertexParseFunc();
    const ByteParseFunc diffuseFunc = _calcDiffuseParseFunc();
    const FloatParseFunc uvFunc = _calcUVParseFunc();
    const FloatParseFunc stFunc = _calcSTParseFunc();
    const FloatParseFunc normalFunc = _calcNormalParseFunc();

    const PolyBuildFunc buildFunc = _calcBuildFunc(mode);
    const IndexParseFunc indexFunc = _calcParseIndexFunc(type);

    const GLsizei type_byte_size = byte_size(type);

    ClipVertex* previous = NULL;
    ClipVertex* firstV = vertex;
    ClipVertex* next = NULL;

    ClipVertex* target = NULL;

    GLsizei i, j = 0;
    GLuint idx;

    if(!indices) {
        GLubyte* vptr = VERTEX_POINTER.ptr + (first * vstride);
        GLubyte* cptr = DIFFUSE_POINTER.ptr + (first * cstride);
        GLubyte* uvptr = UV_POINTER.ptr + (first * uvstride);
        GLubyte* stptr = ST_POINTER.ptr + (first * ststride);
        GLubyte* nptr = NORMAL_POINTER.ptr + (first * nstride);

        for(j = 0; j < count; ++j, ++vertex) {
            if(mode == GL_QUADS) {
                /* Performance optimisation to prevent copying to a temporary */
                GLsizei mod = (j + 1) % 4;
                if(mod == 0) {
                    target = vertex - 1;
                    target->flags = PVR_CMD_VERTEX;
                } else if(mod == 3) {
                    target = vertex + 1;
                    target->flags = PVR_CMD_VERTEX_EOL;
                } else {
                    target = vertex;
                    target->flags = PVR_CMD_VERTEX;
                }
            } else {
                target = vertex;
                target->flags = PVR_CMD_VERTEX;
            }

            vertexFunc(target->xyz, vptr);
            diffuseFunc(target->bgra, cptr);
            vptr += vstride;
            cptr += cstride;

            if(doTexture) {
                uvFunc(target->uv, uvptr);
                uvptr += uvstride;
            }

            if(doMultitexture) {
                stFunc(target->st, stptr);
                stptr += ststride;
            }

            if(doLighting) {
                normalFunc(target->nxyz, nptr);
                nptr += nstride;
            }

            if(mode != GL_QUADS) {
                next = (j < count - 1) ? vertex + 1 : NULL;
                previous = (j > 0) ? vertex - 1 : NULL;
                buildFunc(firstV, previous, vertex, next, j);
            }
        }

    } else {
        for(i = first; i < max; ++i, ++j, ++vertex) {
            if(mode == GL_QUADS) {
                /* Performance optimisation to prevent copying to a temporary */
                GLsizei mod = (j + 1) % 4;
                if(mod == 0) {
                    target = vertex - 1;
                    target->flags = PVR_CMD_VERTEX;
                } else if(mod == 3) {
                    target = vertex + 1;
                    target->flags = PVR_CMD_VERTEX_EOL;
                } else {
                    target = vertex;
                    target->flags = PVR_CMD_VERTEX;
                }
            } else {
                target = vertex;
                target->flags = PVR_CMD_VERTEX;
            }

            idx = (indices) ?
                indexFunc(&indices[type_byte_size * i]) : i;

            vertexFunc(target->xyz, VERTEX_POINTER.ptr + (idx * vstride));
            diffuseFunc(target->bgra, DIFFUSE_POINTER.ptr + (idx * cstride));

            if(doTexture) {
                uvFunc(target->uv, UV_POINTER.ptr + (idx * uvstride));
            }

            if(doMultitexture) {
                stFunc(target->st, ST_POINTER.ptr + (idx * ststride));
            }

            if(doLighting) {
                normalFunc(target->nxyz, NORMAL_POINTER.ptr + (idx * nstride));
            }

            if(mode != GL_QUADS) {
                next = (j < count - 1) ? vertex + 1 : NULL;
                previous = (j > 0) ? vertex - 1 : NULL;
                buildFunc(firstV, previous, vertex, next, j);
            }
        }
    }
}

static void transform(ClipVertex* output, const GLsizei count) {
    /* Perform modelview transform, storing W */

    ClipVertex* vertex = output;

    _applyRenderMatrix(); /* Apply the Render Matrix Stack */

    GLsizei i = count;
    while(i--) {
        register float __x __asm__("fr12") = (vertex->xyz[0]);
        register float __y __asm__("fr13") = (vertex->xyz[1]);
        register float __z __asm__("fr14") = (vertex->xyz[2]);
        register float __w __asm__("fr15");

        __asm__ __volatile__(
            "fldi1 fr15\n"
            "ftrv   xmtrx,fv12\n"
            : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w)
            : "0" (__x), "1" (__y), "2" (__z), "3" (__w)
        );

        vertex->xyz[0] = __x;
        vertex->xyz[1] = __y;
        vertex->xyz[2] = __z;
        vertex->w = __w;

        ++vertex;
    }
}

static GLsizei clip(AlignedVector* polylist, uint32_t offset, const GLsizei count) {
    /* Perform clipping, generating new vertices as necessary */
    clipTriangleStrip2(polylist, offset, _glGetShadeModel() == GL_FLAT);

    /* List size, minus the original offset (which includes the header), minus the header */
    return polylist->size - offset - 1;
}

static void mat_transform3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_single3_nodiv_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += inStride;
        dataOut += outStride;
    }
}

static void mat_transform_normal3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += inStride;
        dataOut += outStride;
    }
}

static void light(ClipVertex* output, const GLsizei count) {
    if(!isLightingEnabled()) {
        return;
    }

    typedef struct {
        float xyz[3];
        float n[3];
    } EyeSpaceData;

    static AlignedVector* eye_space_data = NULL;

    if(!eye_space_data) {
        eye_space_data = (AlignedVector*) malloc(sizeof(AlignedVector));
        aligned_vector_init(eye_space_data, sizeof(EyeSpaceData));
    }

    aligned_vector_resize(eye_space_data, count);

    /* Perform lighting calculations and manipulate the colour */
    ClipVertex* vertex = output;
    EyeSpaceData* eye_space = (EyeSpaceData*) eye_space_data->data;

    _matrixLoadModelView();
    mat_transform3(vertex->xyz, eye_space->xyz, count, sizeof(ClipVertex), sizeof(EyeSpaceData));

    _matrixLoadNormal();
    mat_transform_normal3(vertex->nxyz, eye_space->n, count, sizeof(ClipVertex), sizeof(EyeSpaceData));

    GLsizei i;
    EyeSpaceData* ES = aligned_vector_at(eye_space_data, 0);

    for(i = 0; i < count; ++i, ++vertex, ++ES) {
        /* We ignore diffuse colour when lighting is enabled. If GL_COLOR_MATERIAL is enabled
         * then the lighting calculation should possibly take it into account */

        GLfloat total [] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLfloat to_add [] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLubyte j;
        for(j = 0; j < MAX_LIGHTS; ++j) {
            if(isLightEnabled(j)) {
                _glCalculateLightingContribution(j, ES->xyz, ES->n, vertex->bgra, to_add);

                total[0] += to_add[0];
                total[1] += to_add[1];
                total[2] += to_add[2];
                total[3] += to_add[3];
            }
        }

        vertex->bgra[A8IDX] = (GLubyte) (255.0f * fminf(total[3], 1.0f));
        vertex->bgra[R8IDX] = (GLubyte) (255.0f * fminf(total[0], 1.0f));
        vertex->bgra[G8IDX] = (GLubyte) (255.0f * fminf(total[1], 1.0f));
        vertex->bgra[B8IDX] = (GLubyte) (255.0f * fminf(total[2], 1.0f));
    }
}

static void divide(ClipVertex* output, const GLsizei count) {
    /* Perform perspective divide on each vertex */
    ClipVertex* vertex = output;

    GLsizei i = count;
    while(i--) {
        vertex->xyz[2] = 1.0f / vertex->w;
        vertex->xyz[0] *= vertex->xyz[2];
        vertex->xyz[1] *= vertex->xyz[2];
        ++vertex;
    }
}

static void push(PVRHeader* header, ClipVertex* output, const GLsizei count, PolyList* activePolyList, GLshort textureUnit) {
    // Compile the header
    pvr_poly_cxt_t cxt = *getPVRContext();
    cxt.list_type = activePolyList->list_type;

    _glUpdatePVRTextureContext(&cxt, textureUnit);

    pvr_poly_compile(&header->hdr, &cxt);

    /* Post-process the vertex list */
    ClipVertex* vout = output;

    GLuint i = count;
    while(i--) {
        vout->oargb = 0;
    }
}

#define DEBUG_CLIPPING 0

static void submitVertices(GLenum mode, GLsizei first, GLsizei count, GLenum type, const GLvoid* indices) {
    /* Do nothing if vertices aren't enabled */
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    GLboolean doMultitexture, doTexture, doLighting;
    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);

    glActiveTextureARB(GL_TEXTURE0);
    glGetBooleanv(GL_TEXTURE_2D, &doTexture);

    glActiveTextureARB(GL_TEXTURE1);
    glGetBooleanv(GL_TEXTURE_2D, &doMultitexture);

    doLighting = isLightingEnabled();

    glActiveTextureARB(activeTexture);

    profiler_push(__func__);


    PolyList* activeList = activePolyList();

    /* Make room in the list buffer */
    GLsizei spaceNeeded = (mode == GL_POLYGON || mode == GL_TRIANGLE_FAN) ? ((count - 2) * 3) : count;
    ClipVertex* start = aligned_vector_extend(&activeList->vector, spaceNeeded + 1);

    /* Store a pointer to the header for later */
    PVRHeader* header = (PVRHeader*) start++;

    /* We store an offset to the first ClipVertex because clipping may generate more
     * vertices, which may cause a realloc and thus invalidate start and header
     * we use this startOffset to reset those pointers after clipping */
    uint32_t startOffset = start - (ClipVertex*) activeList->vector.data;

    profiler_checkpoint("allocate");

    generate(start, mode, first, count, (GLubyte*) indices, type, doTexture, doMultitexture, doLighting);

    profiler_checkpoint("generate");

    light(start, spaceNeeded);

    profiler_checkpoint("light");

    transform(start, spaceNeeded);

    profiler_checkpoint("transform");

    if(isClippingEnabled()) {

        uint32_t offset = ((start - 1) - (ClipVertex*) activeList->vector.data);

#if DEBUG_CLIPPING
        uint32_t i = 0;
        fprintf(stderr, "=========\n");

        for(i = offset; i < activeList->vector.size; ++i) {
            ClipVertex* v = aligned_vector_at(&activeList->vector, i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

        spaceNeeded = clip(&activeList->vector, offset, spaceNeeded);

        /* Clipping may have realloc'd so reset the start pointer */
        start = ((ClipVertex*) activeList->vector.data) + startOffset;
        header = (PVRHeader*) (start - 1);  /* Update the header pointer */

#if DEBUG_CLIPPING
        fprintf(stderr, "--------\n");
        for(i = offset; i < activeList->vector.size; ++i) {
            ClipVertex* v = aligned_vector_at(&activeList->vector, i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

    }

    profiler_checkpoint("clip");

    divide(start, spaceNeeded);

    profiler_checkpoint("divide");

    push(header, start, spaceNeeded, activePolyList(), 0);

    profiler_checkpoint("push");
    /*
       Now, if multitexturing is enabled, we want to send exactly the same vertices again, except:
       - We want to enable blending, and send them to the TR list
       - We want to set the depth func to GL_EQUAL
       - We want to set the second texture ID
       - We want to set the uv coordinates to the passed st ones
    */

    if(!doMultitexture) {
        /* Multitexture actively disabled */
        profiler_pop();
        return;
    }

    TextureObject* texture1 = getTexture1();

    if(!texture1 || ((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG)) {
        /* Multitexture implicitly disabled */
        profiler_pop();
        return;
    }

    /* Push back a copy of the list to the transparent poly list, including the header
        (hence the - 1)
    */
    ClipVertex* vertex = aligned_vector_push_back(
        &transparentPolyList()->vector, start - 1, spaceNeeded + 1
    );

    PVRHeader* mtHeader = (PVRHeader*) vertex++;
    ClipVertex* mtStart = vertex;

    /* Copy ST coordinates to UV ones */
    GLsizei i = spaceNeeded;
    while(i--) {
        vertex->uv[0] = vertex->st[0];
        vertex->uv[1] = vertex->st[1];
        ++vertex;
    }

    /* Store state, as we're about to mess around with it */
    GLint depthFunc, blendSrc, blendDst;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_BLEND_SRC, &blendSrc);
    glGetIntegerv(GL_BLEND_DST, &blendDst);

    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);

    glDepthFunc(GL_EQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Send the buffer again to the transparent list */
    push(mtHeader, mtStart, spaceNeeded, transparentPolyList(), 1);

    /* Reset state */
    glDepthFunc(depthFunc);
    glBlendFunc(blendSrc, blendDst);
    (blendEnabled) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    (depthEnabled) ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
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

GLuint _glGetActiveClientTexture() {
    return ACTIVE_CLIENT_TEXTURE;
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
