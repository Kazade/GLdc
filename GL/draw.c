#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <dc/vec3f.h>

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
static GLboolean FAST_PATH_ENABLED = GL_FALSE;

#define ITERATE(count) \
    GLuint i = count; \
    while(i--)


void _glInitAttributePointers() {
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

GL_FORCE_INLINE GLboolean _glIsVertexDataFastPathCompatible() {
    /*
     * We provide a "fast path" if vertex data is provided in
     * exactly the right format that matches what the PVR can handle.
     * This function returns true if all the requirements are met.
     */

    /*
     * At least these attributes need to be enabled, because we're not going to do any checking
     * in the loop
     */
    if((ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG) != VERTEX_ENABLED_FLAG) return GL_FALSE;
    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) return GL_FALSE;
    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) return GL_FALSE;

    // All 3 attribute types must have a stride of 32
    if(VERTEX_POINTER.stride != 32) return GL_FALSE;
    if(UV_POINTER.stride != 32) return GL_FALSE;
    if(DIFFUSE_POINTER.stride != 32) return GL_FALSE;

    // UV must follow vertex, diffuse must follow UV
    if((UV_POINTER.ptr - VERTEX_POINTER.ptr) != sizeof(GLfloat) * 3) return GL_FALSE;
    if((DIFFUSE_POINTER.ptr - UV_POINTER.ptr) != sizeof(GLfloat) * 2) return GL_FALSE;

    if(VERTEX_POINTER.type != GL_FLOAT) return GL_FALSE;
    if(VERTEX_POINTER.size != 3) return GL_FALSE;

    if(UV_POINTER.type != GL_FLOAT) return GL_FALSE;
    if(UV_POINTER.size != 2) return GL_FALSE;

    if(DIFFUSE_POINTER.type != GL_UNSIGNED_BYTE) return GL_FALSE;

    /* BGRA is the required color order */
    if(DIFFUSE_POINTER.size != GL_BGRA) return GL_FALSE;

    return GL_TRUE;
}

GL_FORCE_INLINE GLuint byte_size(GLenum type) {
    switch(type) {
    case GL_BYTE: return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_SHORT: return sizeof(GLshort);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    case GL_INT: return sizeof(GLint);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    case GL_DOUBLE: return sizeof(GLdouble);
    case GL_UNSIGNED_INT_2_10_10_10_REV: return sizeof(GLuint);
    case GL_FLOAT:
    default: return sizeof(GLfloat);
    }
}

typedef void (*FloatParseFunc)(GLfloat* out, const GLubyte* in);
typedef void (*ByteParseFunc)(GLubyte* out, const GLubyte* in);
typedef void (*PolyBuildFunc)(Vertex* first, Vertex* previous, Vertex* vertex, Vertex* next, const GLsizei i);


GL_FORCE_INLINE float clamp(float d, float min, float max) {
    const float t = d < min ? min : d;
    return t > max ? max : t;
}

static void _readVertexData3f3f(const float* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}


GL_FORCE_INLINE float conv_i10_to_norm_float(int i10) {
    struct attr_bits_10 {
        signed int x:10;
    } val;

    val.x = i10;

    return (2.0F * (float)val.x + 1.0F) * (1.0F  / 1023.0F);
}

// 10:10:10:2REV format
static void _readVertexData1i3f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        int inp = *input;
        output[0] = conv_i10_to_norm_float((inp) & 0x3ff);
        output[1] = conv_i10_to_norm_float(((inp) >> 10) & 0x3ff);
        output[2] = conv_i10_to_norm_float(((inp) >> 20) & 0x3ff);

        // fprintf(stderr, "%d -> %f %f %f\n", inp, output[0], output[1], output[2]);

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

/* VE == VertexExtra */
static void _readVertexData3f3fVE(const float* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData3us3f(const GLushort* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData3us3fVE(const GLushort* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData3ui3f(const GLuint* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData3ui3fVE(const GLuint* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData3ub3f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    ITERATE(count) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
        output[2] = input[2] * ONE_OVER_TWO_FIVE_FIVE;

        input += stride;
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData3ub3fVE(const GLubyte* input, GLuint count, GLubyte stride, GLfloat* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    ITERATE(count) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
        output[2] = input[2] * ONE_OVER_TWO_FIVE_FIVE;

        input += stride;
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData2f2f(const float* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2f2fVE(const float* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (float*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData2f3f(const float* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2ub3f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    ITERATE(count) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
        output[2] = 0.0f;

        input += stride;
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2us3f(const GLushort* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2us2f(const GLushort* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2us2fVE(const GLushort* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData2ui2f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2ui2fVE(const GLuint* input, GLuint count, GLubyte stride, GLfloat* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData2ub2f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    ITERATE(count) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;

        input = (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData2ub2fVE(const GLubyte* input, GLuint count, GLubyte stride, GLfloat* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    ITERATE(count) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;

        input = (((GLubyte*) input) + stride);
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData2ui3f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    ITERATE(count) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData4ubARGB(const GLubyte* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[R8IDX] = input[0];
        output[G8IDX] = input[1];
        output[B8IDX] = input[2];
        output[A8IDX] = input[3];

        input += stride;
        output += sizeof(Vertex);
    }
}

static void _readVertexData4fARGB(const float* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
        output[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
        output[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
        output[A8IDX] = (GLubyte) clamp(input[3] * 255.0f, 0, 255);

        input = (float*) (((GLubyte*) input) + stride);
        output += sizeof(Vertex);
    }
}

static void _readVertexData3fARGB(const float* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
        output[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
        output[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
        output[A8IDX] = 1.0f;

        input = (float*) (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData3ubARGB(const GLubyte* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[R8IDX] = input[0];
        output[G8IDX] = input[1];
        output[B8IDX] = input[2];
        output[A8IDX] = 1.0f;

        input = (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _readVertexData4ubRevARGB(const GLubyte* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[B8IDX] = input[0];
        output[G8IDX] = input[1];
        output[R8IDX] = input[2];
        output[A8IDX] = input[3];

        input += stride;
        output += sizeof(Vertex);
    }
}

static void _readVertexData4fRevARGB(const float* input, GLuint count, GLubyte stride, GLubyte* output) {
    ITERATE(count) {
        output[0] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
        output[1] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
        output[2] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
        output[3] = (GLubyte) clamp(input[3] * 255.0f, 0, 255);

        input = (float*) (((GLubyte*) input) + stride);
        output += sizeof(Vertex);
    }
}

static void _fillWithNegZVE(GLuint count, GLfloat* output) {
    ITERATE(count) {
        output[0] = output[1] = 0.0f;
        output[2] = -1.0f;
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

GL_FORCE_INLINE void  _fillWhiteARGB(GLuint count, GLubyte* output) {
    ITERATE(count) {
        output[R8IDX] = 255;
        output[G8IDX] = 255;
        output[B8IDX] = 255;
        output[A8IDX] = 255;

        output += sizeof(Vertex);
    }
}

static void _fillZero2f(GLuint count, GLfloat* output) {
    ITERATE(count) {
        output[0] = output[1] = 0.0f;
        output = (GLfloat*) (((GLubyte*) output) + sizeof(Vertex));
    }
}

static void _fillZero2fVE(GLuint count, GLfloat* output) {
    ITERATE(count) {
        output[0] = output[1] = 0.0f;
        output = (GLfloat*) (((GLubyte*) output) + sizeof(VertexExtra));
    }
}

static void _readVertexData3usARGB(const GLushort* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

static void _readVertexData3uiARGB(const GLuint* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

static void _readVertexData4usARGB(const GLushort* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

static void _readVertexData4uiARGB(const GLuint* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

static void _readVertexData4usRevARGB(const GLushort* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

static void _readVertexData4uiRevARGB(const GLuint* input, GLuint count, GLubyte stride, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(count);
    _GL_UNUSED(stride);
    _GL_UNUSED(output);
    assert(0 && "Not Implemented");
}

GLuint* _glGetEnabledAttributes() {
    return &ENABLED_VERTEX_ATTRIBUTES;
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


GL_FORCE_INLINE IndexParseFunc _calcParseIndexFunc(GLenum type) {
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


GL_FORCE_INLINE void transformToEyeSpace(GLfloat* point) {
    _glMatrixLoadModelView();
    mat_trans_single3_nodiv(point[0], point[1], point[2]);
}

GL_FORCE_INLINE void transformNormalToEyeSpace(GLfloat* normal) {
    _glMatrixLoadNormal();
    mat_trans_normal3(normal[0], normal[1], normal[2]);
}

PVRHeader* _glSubmissionTargetHeader(SubmissionTarget* target) {
    assert(target->header_offset < target->output->vector.size);
    return aligned_vector_at(&target->output->vector, target->header_offset);
}

GL_INLINE_DEBUG Vertex* _glSubmissionTargetStart(SubmissionTarget* target) {
    assert(target->start_offset < target->output->vector.size);
    return aligned_vector_at(&target->output->vector, target->start_offset);
}

Vertex* _glSubmissionTargetEnd(SubmissionTarget* target) {
    return _glSubmissionTargetStart(target) + target->count;
}

static inline void genTriangles(Vertex* output, GLuint count) {
    Vertex* it = output + 2;

    GLuint i;
    for(i = 0; i < count; i += 3) {
        it->flags = PVR_CMD_VERTEX_EOL;
        it += 3;
    }
}

static inline void genQuads(Vertex* output, GLuint count) {
    Vertex* final = output + 3;
    GLuint i;
    for(i = 0; i < count; i += 4) {
        swapVertex((final - 1), final);
        final->flags = PVR_CMD_VERTEX_EOL;
        final += 4;
    }
}

static void genTriangleStrip(Vertex* output, GLuint count) {
    output[count - 1].flags = PVR_CMD_VERTEX_EOL;
}

static void genTriangleFan(Vertex* output, GLuint count) {
    assert(count <= 255);

    Vertex* dst = output + (((count - 2) * 3) - 1);
    Vertex* src = output + (count - 1);

    GLubyte i = count - 2;
    while(i--) {
        *dst = *src--;
        (*dst--).flags = PVR_CMD_VERTEX_EOL;
        *dst-- = *src;
        *dst-- = *output;
    }
}

GL_FORCE_INLINE void _readPositionData(const GLuint first, const GLuint count, Vertex* output) {
    const GLubyte vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    const void* vptr = ((GLubyte*) VERTEX_POINTER.ptr + (first * vstride));

    if(VERTEX_POINTER.size == 3) {
        switch(VERTEX_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData3f3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ub3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3us3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3ui3f(vptr, count, vstride, output[0].xyz);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else if(VERTEX_POINTER.size == 2) {
        switch(VERTEX_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData2f3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui3f(vptr, count, vstride, output[0].xyz);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

GL_FORCE_INLINE void _readUVData(const GLuint first, const GLuint count, Vertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) {
        _fillZero2f(count, output->uv);
        return;
    }

    const GLubyte uvstride = (UV_POINTER.stride) ? UV_POINTER.stride : UV_POINTER.size * byte_size(UV_POINTER.type);
    const void* uvptr = ((GLubyte*) UV_POINTER.ptr + (first * uvstride));

    if(UV_POINTER.size == 2) {
        switch(UV_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData2f2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui2f(uvptr, count, uvstride, output[0].uv);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

GL_FORCE_INLINE void _readSTData(const GLuint first, const GLuint count, VertexExtra* extra) {
    if((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG) {
        _fillZero2fVE(count, extra->st);
        return;
    }

    const GLubyte ststride = (ST_POINTER.stride) ? ST_POINTER.stride : ST_POINTER.size * byte_size(ST_POINTER.type);
    const void* stptr = ((GLubyte*) ST_POINTER.ptr + (first * ststride));

    if(ST_POINTER.size == 2) {
        switch(ST_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData2f2fVE(stptr, count, ststride, extra->st);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub2fVE(stptr, count, ststride, extra->st);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us2fVE(stptr, count, ststride, extra->st);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui2fVE(stptr, count, ststride, extra->st);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

GL_FORCE_INLINE void _readNormalData(const GLuint first, const GLuint count, VertexExtra* extra) {
    if((ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) != NORMAL_ENABLED_FLAG) {
        _fillWithNegZVE(count, extra->nxyz);
        return;
    }

    const GLuint nstride = (NORMAL_POINTER.stride) ? NORMAL_POINTER.stride : NORMAL_POINTER.size * byte_size(NORMAL_POINTER.type);

    const void* nptr = ((GLubyte*) NORMAL_POINTER.ptr + (first * nstride));

    if(NORMAL_POINTER.size == 3 || NORMAL_POINTER.type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        switch(NORMAL_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData3f3fVE(nptr, count, nstride, extra->nxyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ub3fVE(nptr, count, nstride, extra->nxyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3us3fVE(nptr, count, nstride, extra->nxyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3ui3fVE(nptr, count, nstride, extra->nxyz);
            break;
            case GL_UNSIGNED_INT_2_10_10_10_REV:
                _readVertexData1i3f(nptr, count, nstride, extra->nxyz);
            break;
        default:
            fprintf(stderr, "Invalid normal pointer type: %d\n", NORMAL_POINTER.type);
            assert(0 && "Not Implemented");
        }
    } else {
        fprintf(stderr, "Invalid normal pointer type or stride (%d or %d)\n", NORMAL_POINTER.type, NORMAL_POINTER.stride);
        assert(0 && "Not Implemented");
    }

    if(_glIsNormalizeEnabled()) {
        GLubyte* ptr = (GLubyte*) extra->nxyz;
        ITERATE(count) {
            GLfloat* n = (GLfloat*) ptr;
            float temp = n[0] * n[0];
            temp = MATH_fmac(n[1], n[1], temp);
            temp = MATH_fmac(n[2], n[2], temp);

            float ilength = MATH_fsrra(temp);
            n[0] *= ilength;
            n[1] *= ilength;
            n[2] *= ilength;

            ptr += sizeof(VertexExtra);
        }
    }
}

GL_FORCE_INLINE void _readDiffuseData(const GLuint first, const GLuint count, Vertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) {
        /* Just fill the whole thing white if the attribute is disabled */
        _fillWhiteARGB(count, output[0].bgra);
        return;
    }

    const GLuint cstride = (DIFFUSE_POINTER.stride) ? DIFFUSE_POINTER.stride : DIFFUSE_POINTER.size * byte_size(DIFFUSE_POINTER.type);
    const void* cptr = ((GLubyte*) DIFFUSE_POINTER.ptr) + (first * cstride);

    if(DIFFUSE_POINTER.size == 3) {
        switch(DIFFUSE_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData3fARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ubARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3usARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3uiARGB(cptr, count, cstride, output[0].bgra);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else if(DIFFUSE_POINTER.size == 4) {
        switch(DIFFUSE_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData4fARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData4ubARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData4usARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData4uiARGB(cptr, count, cstride, output[0].bgra);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else if(DIFFUSE_POINTER.size == GL_BGRA) {
        switch(DIFFUSE_POINTER.type) {
            case GL_DOUBLE:
            case GL_FLOAT:
                _readVertexData4fRevARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData4ubRevARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData4usRevARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData4uiRevARGB(cptr, count, cstride, output[0].bgra);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    }else {
        assert(0 && "Not Implemented");
    }
}

static void generate(SubmissionTarget* target, const GLenum mode, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type, const GLboolean doTexture, const GLboolean doMultitexture, const GLboolean doLighting) {
    /* Read from the client buffers and generate an array of ClipVertices */
    TRACE();

    static const uint32_t FAST_PATH_BYTE_SIZE = (sizeof(GLfloat) * 3) + (sizeof(GLfloat) * 2) + (sizeof(GLubyte) * 4);
    const GLsizei istride = byte_size(type);

    if(!indices) {
        profiler_push(__func__);

        Vertex* start = _glSubmissionTargetStart(target);

        if(FAST_PATH_ENABLED) {
            /* Copy the pos, uv and color directly in one go */
            const GLubyte* pos = VERTEX_POINTER.ptr;
            Vertex* it = start;
            ITERATE(count) {
                it->flags = PVR_CMD_VERTEX;
                memcpy(it->xyz, pos, FAST_PATH_BYTE_SIZE);
                it++;
                pos += VERTEX_POINTER.stride;
            }
        } else {
            _readPositionData(first, count, start);
            profiler_checkpoint("positions");

            _readDiffuseData(first, count, start);
            profiler_checkpoint("diffuse");

            if(doTexture) _readUVData(first, count, start);

            Vertex* it = _glSubmissionTargetStart(target);

            ITERATE(count) {
                it->flags = PVR_CMD_VERTEX;
                ++it;
            }
        }

        VertexExtra* ve = aligned_vector_at(target->extras, 0);

        if(doLighting) _readNormalData(first, count, ve);
        if(doTexture && doMultitexture) _readSTData(first, count, ve);
        profiler_checkpoint("others");

        // Drawing arrays
        switch(mode) {
        case GL_TRIANGLES:
            genTriangles(start, count);
            break;
        case GL_QUADS:
            genQuads(start, count);
            break;
        case GL_TRIANGLE_FAN:
            genTriangleFan(start, count);
            break;
        case GL_TRIANGLE_STRIP:
            genTriangleStrip(_glSubmissionTargetStart(target), count);
            break;
        default:
            fprintf(stderr, "Unhandled mode %d\n", (int) mode);
            assert(0 && "Not Implemented");
        }

        profiler_checkpoint("quads");
        profiler_pop();
    } else {
        const IndexParseFunc indexFunc = _calcParseIndexFunc(type);
        GLuint j;
        const GLubyte* idx = indices;

        Vertex* vertices = _glSubmissionTargetStart(target);
        VertexExtra* extras = aligned_vector_at(target->extras, 0);

        if(FAST_PATH_ENABLED) {
            typedef struct FastPath {
                float xyz[3];
                float uv[2];
                uint8_t bgra[4];
            } FastPath;

            GLboolean readST = doTexture && doMultitexture;

            ITERATE(count) {
                j = indexFunc(idx);

                vertices->flags = PVR_CMD_VERTEX;

                FastPath* srcV = (FastPath*) ((uint8_t*) VERTEX_POINTER.ptr + (VERTEX_POINTER.stride * j));
                FastPath* dst = (FastPath*) vertices->xyz;
                *dst = *srcV;

                if(doLighting) _readNormalData(j, 1, extras);
                if(readST) _readSTData(j, 1, extras);

                ++vertices;
                ++extras;

                idx += istride;
            }
        } else {
            ITERATE(count) {
                j = indexFunc(idx);
                vertices->flags = PVR_CMD_VERTEX;

                _readPositionData(j, 1, vertices);
                _readDiffuseData(j, 1, vertices);
                if(doTexture) _readUVData(j, 1, vertices);
                if(doLighting) _readNormalData(j, 1, extras);
                if(doTexture && doMultitexture) _readSTData(j, 1, extras);

                ++vertices;
                ++extras;

                idx += istride;
            }
        }

        Vertex* it = _glSubmissionTargetStart(target);
        // Drawing arrays
        switch(mode) {
        case GL_TRIANGLES:
            genTriangles(it, count);
            break;
        case GL_QUADS:
            genQuads(it, count);
            break;
        case GL_TRIANGLE_FAN:
            genTriangleFan(it, count);
            break;
        case GL_TRIANGLE_STRIP:
            genTriangleStrip(it, count);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    }
}

static void transform(SubmissionTarget* target) {
    TRACE();

    /* Perform modelview transform, storing W */
    Vertex* vertex = _glSubmissionTargetStart(target);

    _glApplyRenderMatrix(); /* Apply the Render Matrix Stack */

    ITERATE(target->count) {
        register float __x __asm__("fr12") = (vertex->xyz[0]);
        register float __y __asm__("fr13") = (vertex->xyz[1]);
        register float __z __asm__("fr14") = (vertex->xyz[2]);
        register float __w __asm__("fr15") = (vertex->w);

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

static void clip(SubmissionTarget* target) {
    TRACE();

    /* Perform clipping, generating new vertices as necessary */
    _glClipTriangleStrip(target, _glGetShadeModel() == GL_FLAT);

    /* Reset the count now that we may have added vertices */
    target->count = target->output->vector.size - target->start_offset;
}

static void mat_transform3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;

    ITERATE(count) {
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

    ITERATE(count) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += inStride;
        dataOut += outStride;
    }
}

static void light(SubmissionTarget* target) {

    static AlignedVector* eye_space_data = NULL;

    if(!eye_space_data) {
        eye_space_data = (AlignedVector*) malloc(sizeof(AlignedVector));
        aligned_vector_init(eye_space_data, sizeof(EyeSpaceData));
    }

    aligned_vector_resize(eye_space_data, target->count);

    /* Perform lighting calculations and manipulate the colour */
    Vertex* vertex = _glSubmissionTargetStart(target);
    VertexExtra* extra = aligned_vector_at(target->extras, 0);
    EyeSpaceData* eye_space = (EyeSpaceData*) eye_space_data->data;

    _glMatrixLoadModelView();
    mat_transform3(vertex->xyz, eye_space->xyz, target->count, sizeof(Vertex), sizeof(EyeSpaceData));

    _glMatrixLoadNormal();
    mat_transform_normal3(extra->nxyz, eye_space->n, target->count, sizeof(VertexExtra), sizeof(EyeSpaceData));

    EyeSpaceData* ES = aligned_vector_at(eye_space_data, 0);
    _glPerformLighting(vertex, ES, target->count);
}

GL_FORCE_INLINE void divide(SubmissionTarget* target) {
    TRACE();

    /* Perform perspective divide on each vertex */
    Vertex* vertex = _glSubmissionTargetStart(target);

    ITERATE(target->count) {
        float f = MATH_fsrra(vertex->w * vertex->w);
        vertex->xyz[0] *= f;
        vertex->xyz[1] *= f;
        vertex->xyz[2] = f;

        /* FIXME: Consider taking glDepthRange into account. PVR is designed to use invW rather
         * than Z which is unlike most GPUs - this apparently provides advantages.
         *
         * This can be done (if Z is between -1 and 1) with:
         *
         * //((DEPTH_RANGE_MULTIPLIER_L * vertex->xyz[2] * f) + DEPTH_RANGE_MULTIPLIER_H);
         */
        ++vertex;
    }
}

GL_FORCE_INLINE void push(PVRHeader* header, GLboolean multiTextureHeader, PolyList* activePolyList, GLshort textureUnit) {
    TRACE();

    // Compile the header
    pvr_poly_cxt_t cxt = *_glGetPVRContext();
    cxt.list_type = activePolyList->list_type;

    _glUpdatePVRTextureContext(&cxt, textureUnit);

    if(multiTextureHeader) {
        assert(cxt.list_type == PVR_LIST_TR_POLY);

        cxt.gen.alpha = PVR_ALPHA_ENABLE;
        cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
        cxt.blend.src = PVR_BLEND_ZERO;
        cxt.blend.dst = PVR_BLEND_DESTCOLOR;
        cxt.depth.comparison = PVR_DEPTHCMP_EQUAL;
    }

    pvr_poly_compile(&header->hdr, &cxt);

    /* Post-process the vertex list */
    /*
     * This is currently unnecessary. aligned_vector memsets the allocated objects
     * to zero, and we don't touch oargb, also, we don't *enable* oargb yet in the
     * pvr header so it should be ignored anyway. If this ever becomes a problem,
     * uncomment this.
    ClipVertex* vout = output;
    const ClipVertex* end = output + count;
    while(vout < end) {
        vout->oargb = 0;
    }
    */
}

#define DEBUG_CLIPPING 0

GL_FORCE_INLINE void submitVertices(GLenum mode, GLsizei first, GLuint count, GLenum type, const GLvoid* indices) {
    TRACE();

    /* Do nothing if vertices aren't enabled */
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    /* No vertices? Do nothing */
    if(!count) {
        return;
    }

    if(mode == GL_LINE_STRIP || mode == GL_LINES) {
        fprintf(stderr, "Line drawing is currently unsupported\n");
        return;
    }

    static SubmissionTarget* target = NULL;
    static AlignedVector extras;

    /* Initialization of the target and extras */
    if(!target) {
        target = (SubmissionTarget*) malloc(sizeof(SubmissionTarget));
        target->extras = NULL;
        target->count = 0;
        target->output = NULL;
        target->header_offset = target->start_offset = 0;

        aligned_vector_init(&extras, sizeof(VertexExtra));
        target->extras = &extras;
    }

    GLboolean doMultitexture, doTexture, doLighting;
    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);

    glActiveTextureARB(GL_TEXTURE0);
    glGetBooleanv(GL_TEXTURE_2D, &doTexture);

    glActiveTextureARB(GL_TEXTURE1);
    glGetBooleanv(GL_TEXTURE_2D, &doMultitexture);

    doLighting = _glIsLightingEnabled();

    glActiveTextureARB(activeTexture);

    profiler_push(__func__);

    /* Polygons are treated as triangle fans, the only time this would be a
     * problem is if we supported glPolygonMode(..., GL_LINE) but we don't.
     * We optimise the triangle and quad cases.
     */
    if(mode == GL_POLYGON) {
        if(count == 3) {
            mode = GL_TRIANGLES;
        } else if(count == 4) {
            mode = GL_QUADS;
        } else {
            mode = GL_TRIANGLE_FAN;
        }
    }

    // We don't handle this any further, so just make sure we never pass it down */
    assert(mode != GL_POLYGON);

    target->output = _glActivePolyList();
    target->count = (mode == GL_TRIANGLE_FAN) ? ((count - 2) * 3) : count;
    target->header_offset = target->output->vector.size;
    target->start_offset = target->header_offset + 1;

    assert(target->count);

    /* Make sure we have enough room for all the "extra" data */
    aligned_vector_resize(&extras, target->count);

    /* Make room for the vertices and header */
    aligned_vector_extend(&target->output->vector, target->count + 1);

    profiler_checkpoint("allocate");

    generate(target, mode, first, count, (GLubyte*) indices, type, doTexture, doMultitexture, doLighting);

    profiler_checkpoint("generate");

    if(doLighting){
        light(target);
    }

    profiler_checkpoint("light");

    transform(target);

    profiler_checkpoint("transform");

    if(_glIsClippingEnabled()) {
#if DEBUG_CLIPPING
        uint32_t i = 0;
        fprintf(stderr, "=========\n");

        for(i = 0; i < target->count; ++i) {
            Vertex* v = aligned_vector_at(&target->output->vector, target->start_offset + i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->w, v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

        clip(target);

        assert(extras.size == target->count);

#if DEBUG_CLIPPING
        fprintf(stderr, "--------\n");
        for(i = 0; i < target->count; ++i) {
            Vertex* v = aligned_vector_at(&target->output->vector, target->start_offset + i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->w, v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

    }

    profiler_checkpoint("clip");

    divide(target);

    profiler_checkpoint("divide");

    push(_glSubmissionTargetHeader(target), GL_FALSE, target->output, 0);

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

    TextureObject* texture1 = _glGetTexture1();

    /* Multitexture implicitly disabled */
    if(!texture1 || ((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG)) {
        /* Multitexture actively disabled */
        profiler_pop();
        return;
    }

    /* Push back a copy of the list to the transparent poly list, including the header
        (hence the + 1)
    */
    Vertex* vertex = aligned_vector_push_back(
        &_glTransparentPolyList()->vector, (Vertex*) _glSubmissionTargetHeader(target), target->count + 1
    );

    assert(vertex);

    PVRHeader* mtHeader = (PVRHeader*) vertex++;

    /* Replace the UV coordinates with the ST ones */
    VertexExtra* ve = aligned_vector_at(target->extras, 0);
    ITERATE(target->count) {
        vertex->uv[0] = ve->st[0];
        vertex->uv[1] = ve->st[1];
        ++vertex;
        ++ve;
    }

    /* Send the buffer again to the transparent list */
    push(mtHeader, GL_TRUE, _glTransparentPolyList(), 1);

    profiler_pop();
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }
    _glRecalcFastPath();

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }
    _glRecalcFastPath();

    submitVertices(mode, first, count, GL_UNSIGNED_INT, NULL);
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
        _glKosThrowError(GL_INVALID_ENUM, __func__);
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
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

GLuint _glGetActiveClientTexture() {
    return ACTIVE_CLIENT_TEXTURE;
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}

GLboolean _glRecalcFastPath() {
    FAST_PATH_ENABLED = _glIsVertexDataFastPathCompatible();
    return FAST_PATH_ENABLED;
}

void APIENTRY glTexCoordPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    if(size < 1 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        _glKosPrintError();
        return;
    }

    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &UV_POINTER : &ST_POINTER;

    tointer->ptr = pointer;
    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;
}

void APIENTRY glVertexPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    if(size < 2 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        _glKosPrintError();
        return;
    }

    VERTEX_POINTER.ptr = pointer;
    VERTEX_POINTER.stride = stride;
    VERTEX_POINTER.type = type;
    VERTEX_POINTER.size = size;
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    if(size != 3 && size != 4 && size != GL_BGRA) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        _glKosPrintError();
        return;
    }

    DIFFUSE_POINTER.ptr = pointer;
    DIFFUSE_POINTER.stride = stride;
    DIFFUSE_POINTER.type = type;
    DIFFUSE_POINTER.size = size;
}

void APIENTRY glNormalPointer(GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    GLint validTypes[] = {
        GL_DOUBLE,
        GL_FLOAT,
        GL_BYTE,
        GL_UNSIGNED_BYTE,
        GL_INT,
        GL_UNSIGNED_INT,
        GL_UNSIGNED_INT_2_10_10_10_REV,
        0
    };

    if(_glCheckValidEnum(type, validTypes, __func__) != 0) {
        return;
    }

    NORMAL_POINTER.ptr = pointer;
    NORMAL_POINTER.stride = stride;
    NORMAL_POINTER.type = type;
    NORMAL_POINTER.size = (type == GL_UNSIGNED_INT_2_10_10_10_REV) ? 1 : 3;

}
