#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"


AttribPointerList ATTRIB_POINTERS;
GLuint ENABLED_VERTEX_ATTRIBUTES = 0;
GLuint FAST_PATH_ENABLED = GL_FALSE;

static GLubyte ACTIVE_CLIENT_TEXTURE = 0;
static const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;

extern inline GLuint _glRecalcFastPath();

extern GLboolean AUTOSORT_ENABLED;

#define ITERATE(count) \
    GLuint i = count; \
    while(i--)


void _glInitAttributePointers() {
    TRACE();

    ATTRIB_POINTERS.vertex.ptr = NULL;
    ATTRIB_POINTERS.vertex.stride = 0;
    ATTRIB_POINTERS.vertex.type = GL_FLOAT;
    ATTRIB_POINTERS.vertex.size = 4;

    ATTRIB_POINTERS.colour.ptr = NULL;
    ATTRIB_POINTERS.colour.stride = 0;
    ATTRIB_POINTERS.colour.type = GL_FLOAT;
    ATTRIB_POINTERS.colour.size = 4;

    ATTRIB_POINTERS.uv.ptr = NULL;
    ATTRIB_POINTERS.uv.stride = 0;
    ATTRIB_POINTERS.uv.type = GL_FLOAT;
    ATTRIB_POINTERS.uv.size = 4;

    ATTRIB_POINTERS.st.ptr = NULL;
    ATTRIB_POINTERS.st.stride = 0;
    ATTRIB_POINTERS.st.type = GL_FLOAT;
    ATTRIB_POINTERS.st.size = 4;

    ATTRIB_POINTERS.normal.ptr = NULL;
    ATTRIB_POINTERS.normal.stride = 0;
    ATTRIB_POINTERS.normal.type = GL_FLOAT;
    ATTRIB_POINTERS.normal.size = 3;
}

GL_FORCE_INLINE GLsizei byte_size(GLenum type) {
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

static void _readVertexData3f3f(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    vec3cpy(out, in);
}

// 10:10:10:2REV format
static void _readVertexData1i3f(const GLubyte* in, GLubyte* out) {
    static const float MULTIPLIER = 1.0f / 1023.0f;

    GLfloat* output = (GLfloat*) out;

    union {
        int value;
        struct {
            signed int x: 10;
            signed int y: 10;
            signed int z: 10;
            signed int w: 2;
        } bits;
    } input;

    input.value = *((const GLint*) in);

    output[0] = (2.0f * (float) input.bits.x + 1.0f) * MULTIPLIER;
    output[1] = (2.0f * (float) input.bits.y + 1.0f) * MULTIPLIER;
    output[2] = (2.0f * (float) input.bits.z + 1.0f) * MULTIPLIER;
}

static void _readVertexData3us3f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}

static void _readVertexData3ui3f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}


static void _readVertexData3ub3f(const GLubyte* input, GLubyte* out) {
    float* output = (float*) out;

    output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
    output[2] = input[2] * ONE_OVER_TWO_FIVE_FIVE;
}

static void _readVertexData2f2f(const GLubyte* in, GLubyte* out) {
    vec2cpy(out, in);
}

static void _readVertexData2f3f(const GLubyte* in, GLubyte* out) {
    const float* input = (const float*) in;
    float* output = (float*) out;

    vec2cpy(output, input);
    output[2] = 0.0f;
}

static void _readVertexData2ub3f(const GLubyte* input, GLubyte* out) {
    float* output = (float*) out;

    output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
    output[2] = 0.0f;
}

static void _readVertexData2us3f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = 0.0f;
}

static void _readVertexData2us2f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    float* output = (float*) out;

    output[0] = (float)input[0] / SHRT_MAX;
    output[1] = (float)input[1] / SHRT_MAX;
}

static void _readVertexData2ui2f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
}

static void _readVertexData2ub2f(const GLubyte* input, GLubyte* out) {
    float* output = (float*) out;

    output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
}

static void _readVertexData2ui3f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = 0.0f;
}

static void _readVertexData4ubARGB(const GLubyte* input, GLubyte* output) {
    output[R8IDX] = input[0];
    output[G8IDX] = input[1];
    output[B8IDX] = input[2];
    output[A8IDX] = input[3];
}

static void _readVertexData4fARGB(const GLubyte* in, GLubyte* output) {
    const float* input = (const float*) in;

    output[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
    output[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
    output[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
    output[A8IDX] = (GLubyte) clamp(input[3] * 255.0f, 0, 255);
}

static void _readVertexData3fARGB(const GLubyte* in, GLubyte* output) {
    const float* input = (const float*) in;

    output[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
    output[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
    output[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
    output[A8IDX] = 1.0f;
}

static void _readVertexData3ubARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    output[R8IDX] = input[0];
    output[G8IDX] = input[1];
    output[B8IDX] = input[2];
    output[A8IDX] = 1.0f;
}

static void _readVertexData4ubRevARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    argbcpy(output, input);
}

static void _readVertexData4fRevARGB(const GLubyte* __restrict__ in, GLubyte* __restrict__ output) {
    const float* input = (const float*) in;

    output[0] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
    output[1] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
    output[2] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
    output[3] = (GLubyte) clamp(input[3] * 255.0f, 0, 255);
}

static void _fillWithNegZVE(const GLubyte* __restrict__ input, GLubyte* __restrict__ out) {
    _GL_UNUSED(input);

    typedef struct {
        float x, y, z;
    } V;

    static const V NegZ = {0.0f, 0.0f, -1.0f};

    *((V*) out) = NegZ;
}

static void  _fillWhiteARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    _GL_UNUSED(input);
    *((uint32_t*) output) = ~0;
}

static void _fillZero2f(const GLubyte* __restrict__ input, GLubyte* __restrict__ out) {
    _GL_UNUSED(input);
    memset(out, 0, sizeof(float) * 2);
}

static void _readVertexData3usARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

static void _readVertexData3uiARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

static void _readVertexData4usARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

static void _readVertexData4uiARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

static void _readVertexData4usRevARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

static void _readVertexData4uiRevARGB(const GLubyte* input, GLubyte* output) {
    _GL_UNUSED(input);
    _GL_UNUSED(output);
    gl_assert(0 && "Not Implemented");
}

GLuint* _glGetEnabledAttributes() {
    return &ENABLED_VERTEX_ATTRIBUTES;
}

AttribPointer* _glGetVertexAttribPointer() {
    return &ATTRIB_POINTERS.vertex;
}

AttribPointer* _glGetDiffuseAttribPointer() {
    return &ATTRIB_POINTERS.colour;
}

AttribPointer* _glGetNormalAttribPointer() {
    return &ATTRIB_POINTERS.normal;
}

AttribPointer* _glGetUVAttribPointer() {
    return &ATTRIB_POINTERS.uv;
}

AttribPointer* _glGetSTAttribPointer() {
    return &ATTRIB_POINTERS.st;
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

GL_FORCE_INLINE PolyHeader *_glSubmissionTargetHeader(SubmissionTarget* target) {
    gl_assert(target->header_offset < aligned_vector_size(&target->output->vector));
    return aligned_vector_at(&target->output->vector, target->header_offset);
}

GL_INLINE_DEBUG Vertex* _glSubmissionTargetStart(SubmissionTarget* target) {
    gl_assert(target->start_offset < aligned_vector_size(&target->output->vector));
    return aligned_vector_at(&target->output->vector, target->start_offset);
}

Vertex* _glSubmissionTargetEnd(SubmissionTarget* target) {
    return _glSubmissionTargetStart(target) + target->count;
}

GL_FORCE_INLINE void genTriangles(Vertex* output, GLuint count) {
    Vertex* it = output + 2;

    GLuint i;
    for(i = 0; i < count; i += 3) {
        it->flags = GPU_CMD_VERTEX_EOL;
        it += 3;
    }
}

GL_FORCE_INLINE void genQuads(Vertex* output, GLuint count) {
    Vertex* pen = output + 2;
    Vertex* final = output + 3;
    GLuint i = count >> 2;
    while(i--) {
        PREFETCH(pen + 4);
        PREFETCH(final + 4);

        swapVertex(pen, final);
        final->flags = GPU_CMD_VERTEX_EOL;

        pen += 4;
        final += 4;
    }
}

GL_FORCE_INLINE void genTriangleStrip(Vertex* output, GLuint count) {
    output[count - 1].flags = GPU_CMD_VERTEX_EOL;
}

static void genTriangleFan(Vertex* output, GLuint count) {
    gl_assert(count <= 255);

    Vertex* dst = output + (((count - 2) * 3) - 1);
    Vertex* src = output + (count - 1);

    GLubyte i = count - 2;
    while(i--) {
        *dst = *src--;
        (*dst--).flags = GPU_CMD_VERTEX_EOL;
        *dst-- = *src;
        *dst-- = *output;
    }
}

typedef void (*ReadPositionFunc)(const GLubyte*, GLubyte*);
typedef void (*ReadDiffuseFunc)(const GLubyte*, GLubyte*);
typedef void (*ReadUVFunc)(const GLubyte*, GLubyte*);
typedef void (*ReadNormalFunc)(const GLubyte*, GLubyte*);

ReadPositionFunc calcReadDiffuseFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) {
        /* Just fill the whole thing white if the attribute is disabled */
        return _fillWhiteARGB;
    }

    switch(ATTRIB_POINTERS.colour.type) {
        default:
        case GL_DOUBLE:
        case GL_FLOAT:
            return (ATTRIB_POINTERS.colour.size == 3) ? _readVertexData3fARGB:
                   (ATTRIB_POINTERS.colour.size == 4) ? _readVertexData4fARGB:
                    _readVertexData4fRevARGB;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return (ATTRIB_POINTERS.colour.size == 3) ? _readVertexData3ubARGB:
                   (ATTRIB_POINTERS.colour.size == 4) ? _readVertexData4ubARGB:
                    _readVertexData4ubRevARGB;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return (ATTRIB_POINTERS.colour.size == 3) ? _readVertexData3usARGB:
                   (ATTRIB_POINTERS.colour.size == 4) ? _readVertexData4usARGB:
                    _readVertexData4usRevARGB;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return (ATTRIB_POINTERS.colour.size == 3) ? _readVertexData3uiARGB:
                   (ATTRIB_POINTERS.colour.size == 4) ? _readVertexData4uiARGB:
                    _readVertexData4uiRevARGB;
    }
}

ReadPositionFunc calcReadPositionFunc() {
    switch(ATTRIB_POINTERS.vertex.type) {
        default:
        case GL_DOUBLE:
        case GL_FLOAT:
            return (ATTRIB_POINTERS.vertex.size == 3) ? _readVertexData3f3f:
                    _readVertexData2f3f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return (ATTRIB_POINTERS.vertex.size == 3) ? _readVertexData3ub3f:
                    _readVertexData2ub3f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return (ATTRIB_POINTERS.vertex.size == 3) ? _readVertexData3us3f:
                    _readVertexData2us3f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return (ATTRIB_POINTERS.vertex.size == 3) ? _readVertexData3ui3f:
                    _readVertexData2ui3f;
    }
}

ReadUVFunc calcReadUVFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) {
        return _fillZero2f;
    }

    switch(ATTRIB_POINTERS.uv.type) {
        default:
        case GL_DOUBLE:
        case GL_FLOAT:
            return _readVertexData2f2f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readVertexData2ub2f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readVertexData2us2f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readVertexData2ui2f;
    }
}

ReadUVFunc calcReadSTFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG) {
        return _fillZero2f;
    }

    switch(ATTRIB_POINTERS.st.type) {
        default:
        case GL_DOUBLE:
        case GL_FLOAT:
            return _readVertexData2f2f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readVertexData2ub2f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readVertexData2us2f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readVertexData2ui2f;
    }
}

ReadNormalFunc calcReadNormalFunc() {
    if((ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) != NORMAL_ENABLED_FLAG) {
        return _fillWithNegZVE;
    }

    switch(ATTRIB_POINTERS.normal.type) {
        default:
        case GL_DOUBLE:
        case GL_FLOAT:
            return _readVertexData3f3f;
        break;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readVertexData3ub3f;
        break;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readVertexData3us3f;
        break;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readVertexData3ui3f;
        break;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return _readVertexData1i3f;
        break;
    }
}

static void _readPositionData(ReadDiffuseFunc func, const GLuint first, const GLuint count, Vertex* it) {
    const GLsizei vstride = ATTRIB_POINTERS.vertex.stride;
    const GLubyte* vptr = ((GLubyte*) ATTRIB_POINTERS.vertex.ptr + (first * vstride));

    float pos[3];

    ITERATE(count) {
        PREFETCH(vptr + vstride);
        func(vptr, (GLubyte*) pos);
        it->flags = GPU_CMD_VERTEX;

        vptr += vstride;
        ++it;
    }
}

static void _readUVData(ReadUVFunc func, const GLuint first, const GLuint count, Vertex* it) {
    const GLsizei uvstride = ATTRIB_POINTERS.uv.stride;
    const GLubyte* uvptr = ((GLubyte*) ATTRIB_POINTERS.uv.ptr + (first * uvstride));

    ITERATE(count) {
        PREFETCH(uvptr + uvstride);

        func(uvptr, (GLubyte*) it->uv);
        uvptr += uvstride;
        ++it;
    }
}

static void _readSTData(ReadUVFunc func, const GLuint first, const GLuint count, VertexExtra* it) {
    const GLsizei ststride = ATTRIB_POINTERS.st.stride;
    const GLubyte* stptr = ((GLubyte*) ATTRIB_POINTERS.st.ptr + (first * ststride));

    ITERATE(count) {
        PREFETCH(stptr + ststride);
        func(stptr, (GLubyte*) it->st);
        stptr += ststride;
        ++it;
    }
}

static void _readNormalData(ReadNormalFunc func, const GLuint first, const GLuint count, VertexExtra* it) {
    const GLsizei nstride = ATTRIB_POINTERS.normal.stride;
    const GLubyte* nptr = ((GLubyte*) ATTRIB_POINTERS.normal.ptr + (first * nstride));

    ITERATE(count) {
        func(nptr, (GLubyte*) it->nxyz);
        nptr += nstride;

        if(_glIsNormalizeEnabled()) {
            GLfloat* n = (GLfloat*) it->nxyz;
            float temp = n[0] * n[0];
            temp = MATH_fmac(n[1], n[1], temp);
            temp = MATH_fmac(n[2], n[2], temp);

            float ilength = MATH_fsrra(temp);
            n[0] *= ilength;
            n[1] *= ilength;
            n[2] *= ilength;
        }

        ++it;
    }
}

GL_FORCE_INLINE GLuint diffusePointerSize() {
    return (ATTRIB_POINTERS.colour.size == GL_BGRA) ? 4 : ATTRIB_POINTERS.colour.size;
}

static void _readDiffuseData(ReadDiffuseFunc func, const GLuint first, const GLuint count, Vertex* it) {
    const GLuint cstride = ATTRIB_POINTERS.colour.stride;
    const GLubyte* cptr = ((GLubyte*) ATTRIB_POINTERS.colour.ptr) + (first * cstride);

    ITERATE(count) {
        PREFETCH(cptr + cstride);
        func(cptr, it->bgra);
        cptr += cstride;
        ++it;
    }
}

static void generateElements(
        SubmissionTarget* target, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {

    const GLsizei istride = byte_size(type);
    const IndexParseFunc IndexFunc = _calcParseIndexFunc(type);

    GLubyte* xyz;
    GLubyte* uv;
    GLubyte* bgra;
    GLubyte* st;
    GLubyte* nxyz;

    Vertex* output = _glSubmissionTargetStart(target);
    VertexExtra* ve = aligned_vector_at(target->extras, 0);

    uint32_t i = first;
    uint32_t idx = 0;

    const ReadPositionFunc pos_func = calcReadPositionFunc();
    const GLsizei vstride = ATTRIB_POINTERS.vertex.stride;

    const ReadUVFunc uv_func = calcReadUVFunc();
    const GLuint uvstride = ATTRIB_POINTERS.uv.stride;

    const ReadUVFunc st_func = calcReadSTFunc();
    const GLuint ststride = ATTRIB_POINTERS.st.stride;

    const ReadDiffuseFunc diffuse_func = calcReadDiffuseFunc();
    const GLuint dstride = ATTRIB_POINTERS.colour.stride;

    const ReadNormalFunc normal_func = calcReadNormalFunc();
    const GLuint nstride = ATTRIB_POINTERS.normal.stride;

    for(; i < first + count; ++i) {
        idx = IndexFunc(indices + (i * istride));

        xyz = (GLubyte*) ATTRIB_POINTERS.vertex.ptr + (idx * vstride);
        uv = (GLubyte*) ATTRIB_POINTERS.uv.ptr + (idx * uvstride);
        bgra = (GLubyte*) ATTRIB_POINTERS.colour.ptr + (idx * dstride);
        st = (GLubyte*) ATTRIB_POINTERS.st.ptr + (idx * ststride);
        nxyz = (GLubyte*) ATTRIB_POINTERS.normal.ptr + (idx * nstride);

        pos_func(xyz, (GLubyte*) output->xyz);
        uv_func(uv, (GLubyte*) output->uv);
        diffuse_func(bgra, output->bgra);
        st_func(st, (GLubyte*) ve->st);
        normal_func(nxyz, (GLubyte*) ve->nxyz);

        output->flags = GPU_CMD_VERTEX;
        ++output;
        ++ve;
    }
}

typedef struct {
    float x, y, z;
} Float3;

typedef struct {
    float u, v;
} Float2;

static const Float3 F3Z = {0.0f, 0.0f, 1.0f};
static const Float2 F2ZERO = {0.0f, 0.0f};

static void generateElementsFastPath(
        SubmissionTarget* target, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {

    Vertex* start = _glSubmissionTargetStart(target);

    const GLuint vstride = ATTRIB_POINTERS.vertex.stride;
    const GLuint uvstride = ATTRIB_POINTERS.uv.stride;
    const GLuint ststride = ATTRIB_POINTERS.st.stride;
    const GLuint dstride = ATTRIB_POINTERS.colour.stride;
    const GLuint nstride = ATTRIB_POINTERS.normal.stride;

    const GLsizei istride = byte_size(type);
    const IndexParseFunc IndexFunc = _calcParseIndexFunc(type);

    /* Copy the pos, uv and color directly in one go */
    const GLubyte* pos = (ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG) ? ATTRIB_POINTERS.vertex.ptr : NULL;
    const GLubyte* uv = (ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) ? ATTRIB_POINTERS.uv.ptr : NULL;
    const GLubyte* col = (ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) ? ATTRIB_POINTERS.colour.ptr : NULL;
    const GLubyte* st = (ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) ? ATTRIB_POINTERS.st.ptr : NULL;
    const GLubyte* n = (ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) ? ATTRIB_POINTERS.normal.ptr : NULL;

    VertexExtra* ve = aligned_vector_at(target->extras, 0);
    Vertex* it = start;

    const float w = 1.0f;

    if(!pos) {
        return;
    }

    for(GLuint i = first; i < first + count; ++i) {
        GLuint idx = IndexFunc(indices + (i * istride));

        it->flags = GPU_CMD_VERTEX;

        pos = (GLubyte*) ATTRIB_POINTERS.vertex.ptr + (idx * vstride);
        TransformVertex((const float*) pos, &w, it->xyz, &it->w);

        if(uv) {
            uv = (GLubyte*) ATTRIB_POINTERS.uv.ptr + (idx * uvstride);
            MEMCPY4(it->uv, uv, sizeof(float) * 2);
        } else {
            *((Float2*) it->uv) = F2ZERO;
        }

        if(col) {
            col = (GLubyte*) ATTRIB_POINTERS.colour.ptr + (idx * dstride);
            MEMCPY4(it->bgra, col, sizeof(uint32_t));
        } else {
            *((uint32_t*) it->bgra) = ~0;
        }

        if(st) {
            st = (GLubyte*) ATTRIB_POINTERS.st.ptr + (idx * ststride);
            MEMCPY4(ve->st, st, sizeof(float) * 2);
        } else {
            *((Float2*) ve->st) = F2ZERO;
        }

        if(n) {
            n = (GLubyte*) ATTRIB_POINTERS.normal.ptr + (idx * nstride);
            MEMCPY4(ve->nxyz, n, sizeof(float) * 3);
        } else {
            *((Float3*) ve->nxyz) = F3Z;
        }

        it++;
        ve++;
    }
}

#define likely(x)      __builtin_expect(!!(x), 1)

#define POLYMODE ALL
#define PROCESS_VERTEX_FLAGS(it, i) { \
    (it)->flags = GPU_CMD_VERTEX; \
}

#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

#define POLYMODE QUADS
#define PROCESS_VERTEX_FLAGS(it, i) { \
    it->flags = GPU_CMD_VERTEX; \
    if(((i + 1) % 4) == 0) { \
        Vertex t = *it; \
        *it = *(it - 1); \
        *(it - 1) = t; \
        it->flags = GPU_CMD_VERTEX_EOL; \
    } \
}

#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

#define POLYMODE TRIS
#define PROCESS_VERTEX_FLAGS(it, i) { \
    it->flags = ((i + 1) % 3 == 0) ? GPU_CMD_VERTEX_EOL : GPU_CMD_VERTEX; \
}
#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

static void generateArrays(SubmissionTarget* target, const GLsizei first, const GLuint count) {
    Vertex* start = _glSubmissionTargetStart(target);
    VertexExtra* ve = aligned_vector_at(target->extras, 0);

    const ReadPositionFunc pfunc = calcReadPositionFunc();
    const ReadDiffuseFunc dfunc = calcReadDiffuseFunc();
    const ReadUVFunc uvfunc = calcReadUVFunc();
    const ReadNormalFunc nfunc = calcReadNormalFunc();
    const ReadUVFunc stfunc = calcReadSTFunc();

    _readPositionData(pfunc, first, count, start);
    _readDiffuseData(dfunc, first, count, start);
    _readUVData(uvfunc, first, count, start);
    _readNormalData(nfunc, first, count, ve);
    _readSTData(stfunc, first, count, ve);
}

static void generate(SubmissionTarget* target, const GLenum mode, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {
    /* Read from the client buffers and generate an array of ClipVertices */
    TRACE();

    if(FAST_PATH_ENABLED) {
        if(indices) {
            generateElementsFastPath(target, first, count, indices, type);
        } else {
            switch(mode) {
                case GL_QUADS:
                    generateArraysFastPath_QUADS(target, first, count);
                    return;  // Don't need to do any more processing
                case GL_TRIANGLES:
                    generateArraysFastPath_TRIS(target, first, count);
                    return; // Don't need to do any more processing
                default:
                    generateArraysFastPath_ALL(target, first, count);
            }
        }
    } else {
        if(indices) {
            generateElements(target, first, count, indices, type);
        } else {
            generateArrays(target, first, count);
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
        gl_assert(0 && "Not Implemented");
    }
}

static void transform(SubmissionTarget* target) {
    TRACE();

    /* Perform modelview transform, storing W */
    Vertex* vertex = _glSubmissionTargetStart(target);

    TransformVertices(vertex, target->count);
}

static void mat_transform_normal3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    const uint8_t* dataIn = (const uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;

    ITERATE(count) {
        const float* in = (const float*) dataIn;
        float* out = (float*) dataOut;

        TransformNormalNoMod(in, out);

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

    _glMatrixLoadNormal();
    mat_transform_normal3(extra->nxyz, eye_space->n, target->count, sizeof(VertexExtra), sizeof(EyeSpaceData));

    EyeSpaceData* ES = aligned_vector_at(eye_space_data, 0);
    _glPerformLighting(vertex, ES, target->count);
}

GL_FORCE_INLINE void divide(SubmissionTarget* target) {
    TRACE();

    /* Perform perspective divide on each vertex */
    Vertex* vertex = _glSubmissionTargetStart(target);

    const float h = GetVideoMode()->height;

    ITERATE(target->count) {
        const float f = MATH_Fast_Invert(vertex->w);

        /* Convert to NDC and apply viewport */
        vertex->xyz[0] = MATH_fmac(
            VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
        );
        vertex->xyz[1] = h - MATH_fmac(
            VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
        );

        /* Apply depth range */
        vertex->xyz[2] = MAX(
            1.0f - MATH_fmac(vertex->xyz[2] * f, 0.5f, 0.5f),
            PVR_MIN_Z
        );

        ++vertex;
    }
}

GL_FORCE_INLINE int _calc_pvr_face_culling() {
    if(!_glIsCullingEnabled()) {
        return GPU_CULLING_SMALL;
    } else {
        if(_glGetCullFace() == GL_BACK) {
            return (_glGetFrontFace() == GL_CW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        } else {
            return (_glGetFrontFace() == GL_CCW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        }
    }
}

GL_FORCE_INLINE int _calc_pvr_depth_test() {
    if(!_glIsDepthTestEnabled()) {
        return GPU_DEPTHCMP_ALWAYS;
    }

    switch(_glGetDepthFunc()) {
        case GL_NEVER:
            return GPU_DEPTHCMP_NEVER;
        case GL_LESS:
            return GPU_DEPTHCMP_GREATER;
        case GL_EQUAL:
            return GPU_DEPTHCMP_EQUAL;
        case GL_LEQUAL:
            return GPU_DEPTHCMP_GEQUAL;
        case GL_GREATER:
            return GPU_DEPTHCMP_LESS;
        case GL_NOTEQUAL:
            return GPU_DEPTHCMP_NOTEQUAL;
        case GL_GEQUAL:
            return GPU_DEPTHCMP_LEQUAL;
        break;
        case GL_ALWAYS:
        default:
            return GPU_DEPTHCMP_ALWAYS;
    }
}

GL_FORCE_INLINE int _calcPVRBlendFactor(GLenum factor) {
    switch(factor) {
    case GL_ZERO:
        return GPU_BLEND_ZERO;
    case GL_SRC_ALPHA:
        return GPU_BLEND_SRCALPHA;
    case GL_DST_COLOR:
        return GPU_BLEND_DESTCOLOR;
    case GL_DST_ALPHA:
        return GPU_BLEND_DESTALPHA;
    case GL_ONE_MINUS_DST_COLOR:
        return GPU_BLEND_INVDESTCOLOR;
    case GL_ONE_MINUS_SRC_ALPHA:
        return GPU_BLEND_INVSRCALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return GPU_BLEND_INVDESTALPHA;
    case GL_ONE:
        return GPU_BLEND_ONE;
    default:
        fprintf(stderr, "Invalid blend mode: %u\n", (unsigned int) factor);
        return GPU_BLEND_ONE;
    }
}


GL_FORCE_INLINE void _updatePVRBlend(PolyContext* context) {
    if(_glIsBlendingEnabled() || _glIsAlphaTestEnabled()) {
        context->gen.alpha = GPU_ALPHA_ENABLE;
    } else {
        context->gen.alpha = GPU_ALPHA_DISABLE;
    }

    context->blend.src = _calcPVRBlendFactor(_glGetBlendSourceFactor());
    context->blend.dst = _calcPVRBlendFactor(_glGetBlendDestFactor());
}

GL_FORCE_INLINE void apply_poly_header(PolyHeader* header, GLboolean multiTextureHeader, PolyList* activePolyList, GLshort textureUnit) {
    TRACE();

    // Compile the header
    PolyContext ctx;
    memset(&ctx, 0, sizeof(PolyContext));

    ctx.list_type = activePolyList->list_type;
    ctx.fmt.color = GPU_CLRFMT_ARGBPACKED;
    ctx.fmt.uv = GPU_UVFMT_32BIT;
    ctx.gen.color_clamp = GPU_CLRCLAMP_DISABLE;

    ctx.gen.culling = _calc_pvr_face_culling();
    ctx.depth.comparison = _calc_pvr_depth_test();
    ctx.depth.write = _glIsDepthWriteEnabled() ? GPU_DEPTHWRITE_ENABLE : GPU_DEPTHWRITE_DISABLE;

    ctx.gen.shading = (_glGetShadeModel() == GL_SMOOTH) ? GPU_SHADE_GOURAUD : GPU_SHADE_FLAT;

    if(_glIsScissorTestEnabled()) {
        ctx.gen.clip_mode = GPU_USERCLIP_INSIDE;
    } else {
        ctx.gen.clip_mode = GPU_USERCLIP_DISABLE;
    }

    if(_glIsFogEnabled()) {
        ctx.gen.fog_type = GPU_FOG_TABLE;
    } else {
        ctx.gen.fog_type = GPU_FOG_DISABLE;
    }

    _updatePVRBlend(&ctx);

    if(ctx.list_type == GPU_LIST_OP_POLY) {
        /* Opaque polys are always one/zero */
        ctx.blend.src = GPU_BLEND_ONE;
        ctx.blend.dst = GPU_BLEND_ZERO;
    } else if(ctx.list_type == GPU_LIST_PT_POLY) {
        /* Punch-through polys require fixed blending and depth modes */
        ctx.blend.src = GPU_BLEND_SRCALPHA;
        ctx.blend.dst = GPU_BLEND_INVSRCALPHA;
        ctx.depth.comparison = GPU_DEPTHCMP_LEQUAL;
    } else if(ctx.list_type == GPU_LIST_TR_POLY && AUTOSORT_ENABLED) {
        /* Autosort mode requires this mode for transparent polys */
        ctx.depth.comparison = GPU_DEPTHCMP_GEQUAL;
    }

    _glUpdatePVRTextureContext(&ctx, textureUnit);

    if(multiTextureHeader) {
        gl_assert(ctx.list_type == GPU_LIST_TR_POLY);

        ctx.gen.alpha = GPU_ALPHA_ENABLE;
        ctx.txr.alpha = GPU_TXRALPHA_ENABLE;
        ctx.blend.src = GPU_BLEND_ZERO;
        ctx.blend.dst = GPU_BLEND_DESTCOLOR;
        ctx.depth.comparison = GPU_DEPTHCMP_EQUAL;
    }

    CompilePolyHeader(header, &ctx);

    /* Force bits 18 and 19 on to switch to 6 triangle strips */
    header->cmd |= 0xC0000;

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


static AlignedVector VERTEX_EXTRAS;
static SubmissionTarget SUBMISSION_TARGET;


void _glInitSubmissionTarget() {
    SubmissionTarget* target = &SUBMISSION_TARGET;

    target->extras = NULL;
    target->count = 0;
    target->output = NULL;
    target->header_offset = target->start_offset = 0;

    aligned_vector_init(&VERTEX_EXTRAS, sizeof(VertexExtra));
    target->extras = &VERTEX_EXTRAS;
}


GL_FORCE_INLINE void submitVertices(GLenum mode, GLsizei first, GLuint count, GLenum type, const GLvoid* indices) {

    SubmissionTarget* const target = &SUBMISSION_TARGET;
    AlignedVector* const extras = target->extras;

    TRACE();

    /* Do nothing if vertices aren't enabled */
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    /* No vertices? Do nothing */
    if(!count) {
        return;
    }

    /* Polygons are treated as triangle fans, the only time this would be a
     * problem is if we supported glPolygonMode(..., GL_LINE) but we don't.
     * We optimise the triangle and quad cases.
     */
    if(mode == GL_POLYGON) {
        switch(count) {
            case 2:
                mode = GL_LINES;
            break;
            case 3:
                mode = GL_TRIANGLES;
            break;
            case 4:
                mode = GL_QUADS;
            break;
            default:
                mode = GL_TRIANGLE_FAN;
        }
    }

    if(mode == GL_LINE_STRIP || mode == GL_LINES) {
        fprintf(stderr, "Line drawing is currently unsupported\n");
        return;
    }

    // We don't handle this any further, so just make sure we never pass it down */
    gl_assert(mode != GL_POLYGON);

    target->output = _glActivePolyList();
    gl_assert(target->output);
    gl_assert(extras);

    uint32_t vector_size = aligned_vector_size(&target->output->vector);

    GLboolean header_required = (vector_size == 0) || _glGPUStateIsDirty();

    target->count = (mode == GL_TRIANGLE_FAN) ? ((count - 2) * 3) : count;
    target->header_offset = vector_size;
    target->start_offset = target->header_offset + (header_required ? 1 : 0);

    gl_assert(target->start_offset >= target->header_offset);
    gl_assert(target->count);

    /* Make sure we have enough room for all the "extra" data */
    aligned_vector_resize(extras, target->count);

    /* Make room for the vertices and header */
    aligned_vector_extend(&target->output->vector, target->count + (header_required));

    if(header_required) {
        apply_poly_header(_glSubmissionTargetHeader(target), GL_FALSE, target->output, 0);
        _glGPUStateMarkClean();
    }

    /* If we're lighting, then we need to do some work in
     * eye-space, so we only transform vertices by the modelview
     * matrix, and then later multiply by projection.
     *
     * If we're not doing lighting though we can optimise by taking
     * vertices straight to clip-space */

    if(_glIsLightingEnabled()) {
        _glMatrixLoadModelView();
    } else {
        _glMatrixLoadModelViewProjection();
    }

    /* If we're FAST_PATH_ENABLED, then this will do the transform for us */
    generate(target, mode, first, count, (GLubyte*) indices, type);

    /* No fast path, then we have to do another iteration :( */
    if(!FAST_PATH_ENABLED) {
        /* Multiply by modelview */
        transform(target);
    }

    if(_glIsLightingEnabled()){
        light(target);

        /* OK eye-space work done, now move into clip space */
        _glMatrixLoadProjection();
        transform(target);
    }

    // /*
    //    Now, if multitexturing is enabled, we want to send exactly the same vertices again, except:
    //    - We want to enable blending, and send them to the TR list
    //    - We want to set the depth func to GL_EQUAL
    //    - We want to set the second texture ID
    //    - We want to set the uv coordinates to the passed st ones
    // */

    // if(!TEXTURES_ENABLED[1]) {
    //     /* Multitexture actively disabled */
    //     return;
    // }

    // TextureObject* texture1 = _glGetTexture1();

    // /* Multitexture implicitly disabled */
    // if(!texture1 || ((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG)) {
    //     /* Multitexture actively disabled */
    //     return;
    // }

    // /* Push back a copy of the list to the transparent poly list, including the header
    //     (hence the + 1)
    // */
    // Vertex* vertex = aligned_vector_push_back(
    //     &_glTransparentPolyList()->vector, (Vertex*) _glSubmissionTargetHeader(target), target->count + 1
    // );

    // gl_assert(vertex);

    // PolyHeader* mtHeader = (PolyHeader*) vertex++;
    // /* Send the buffer again to the transparent list */
    // apply_poly_header(mtHeader, GL_TRUE, _glTransparentPolyList(), 1);

    // /* Replace the UV coordinates with the ST ones */
    // VertexExtra* ve = aligned_vector_at(target->extras, 0);
    // ITERATE(target->count) {
    //     vertex->uv[0] = ve->st[0];
    //     vertex->uv[1] = ve->st[1];
    //     ++vertex;
    //     ++ve;
    // }
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

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

    /* It's possible that we called glVertexPointer and friends before
     * calling glEnableClientState, so we should recheck to make sure
     * everything is in the right format with this new information */
    _glRecalcFastPath();
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

    /* State changed, recalculate */
    _glRecalcFastPath();
}

GLuint _glGetActiveClientTexture() {
    return ACTIVE_CLIENT_TEXTURE;
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_GLDC_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}

GL_FORCE_INLINE GLboolean _glComparePointers(AttribPointer* p, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    return (p->size == size && p->type == type && p->stride == stride && p->ptr == pointer);
}

void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    TRACE();

    if(size < 1 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    stride = (stride) ? stride : size * byte_size(type);

    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &ATTRIB_POINTERS.uv : &ATTRIB_POINTERS.st;

    if(_glComparePointers(tointer, size, type, stride, pointer)) {
        // No Change
        return;
    }

    tointer->ptr = pointer;
    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;

    _glRecalcFastPath();
}

void APIENTRY glVertexPointer(GLint size, GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    if(size < 2 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    stride = (stride) ? stride : (size * byte_size(ATTRIB_POINTERS.vertex.type));

    if(_glComparePointers(&ATTRIB_POINTERS.vertex, size, type, stride, pointer)) {
        // No Change
        return;
    }

    ATTRIB_POINTERS.vertex.ptr = pointer;
    ATTRIB_POINTERS.vertex.stride = stride;
    ATTRIB_POINTERS.vertex.type = type;
    ATTRIB_POINTERS.vertex.size = size;

    _glRecalcFastPath();
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    if(size != 3 && size != 4 && size != GL_BGRA) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    stride = (stride) ? stride : ((size == GL_BGRA) ? 4 : size) * byte_size(type);

    if(_glComparePointers(&ATTRIB_POINTERS.colour, size, type, stride, pointer)) {
        // No Change
        return;
    }

    ATTRIB_POINTERS.colour.ptr = pointer;
    ATTRIB_POINTERS.colour.type = type;
    ATTRIB_POINTERS.colour.size = size;
    ATTRIB_POINTERS.colour.stride = stride;

    _glRecalcFastPath();
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

    stride = (stride) ? stride : ATTRIB_POINTERS.normal.size * byte_size(type);

    if(_glComparePointers(&ATTRIB_POINTERS.normal, 3, type, stride, pointer)) {
        // No Change
        return;
    }

    ATTRIB_POINTERS.normal.ptr = pointer;
    ATTRIB_POINTERS.normal.size = (type == GL_UNSIGNED_INT_2_10_10_10_REV) ? 1 : 3;
    ATTRIB_POINTERS.normal.stride = stride;
    ATTRIB_POINTERS.normal.type = type;

    _glRecalcFastPath();
}
