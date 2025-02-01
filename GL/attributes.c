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

static const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;

extern inline GLuint _glRecalcFastPath();

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
    output[A8IDX] = 255;
}

static void _readVertexData3ubARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    output[R8IDX] = input[0];
    output[G8IDX] = input[1];
    output[B8IDX] = input[2];
    output[A8IDX] = 255;
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

static ReadAttributeFunc calcReadDiffuseFunc() {
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

static ReadAttributeFunc calcReadPositionFunc() {
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

static ReadAttributeFunc calcReadUVFunc() {
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

static ReadAttributeFunc calcReadSTFunc() {
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

static ReadAttributeFunc calcReadNormalFunc() {
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


// Used to avoid checking and updating attribute related state unless necessary
GL_FORCE_INLINE GLboolean _glStateUnchanged(AttribPointer* p, GLint size, GLenum type, GLsizei stride) {
    return (p->size == size && p->type == type && p->stride == stride);
}

void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    TRACE();

    stride = (stride) ? stride : size * byte_size(type);
    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &ATTRIB_POINTERS.uv : &ATTRIB_POINTERS.st;
    tointer->ptr = pointer;

    if(_glStateUnchanged(tointer, size, type, stride)) return;

    if(size < 1 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;

	ATTRIB_POINTERS.uv_func = calcReadUVFunc();
	ATTRIB_POINTERS.st_func = calcReadSTFunc();
    _glRecalcFastPath();
}

void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    TRACE();

    stride = (stride) ? stride : (size * byte_size(type));
    ATTRIB_POINTERS.vertex.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_POINTERS.vertex, size, type, stride)) return;

    if(size < 2 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    ATTRIB_POINTERS.vertex.stride = stride;
    ATTRIB_POINTERS.vertex.type = type;
    ATTRIB_POINTERS.vertex.size = size;
	ATTRIB_POINTERS.vertex_func = calcReadPositionFunc();

    _glRecalcFastPath();
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    stride = (stride) ? stride : ((size == GL_BGRA) ? 4 : size) * byte_size(type);
    ATTRIB_POINTERS.colour.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_POINTERS.colour, size, type, stride)) return;

    if(size != 3 && size != 4 && size != GL_BGRA) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    ATTRIB_POINTERS.colour.type = type;
    ATTRIB_POINTERS.colour.size = size;
    ATTRIB_POINTERS.colour.stride = stride;
	ATTRIB_POINTERS.colour_func = calcReadDiffuseFunc();

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

    stride = (stride) ? stride : ATTRIB_POINTERS.normal.size * byte_size(type);
    ATTRIB_POINTERS.normal.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_POINTERS.normal, 3, type, stride)) return;

    if(_glCheckValidEnum(type, validTypes, __func__) != 0) {
        return;
    }

    ATTRIB_POINTERS.normal.size = (type == GL_UNSIGNED_INT_2_10_10_10_REV) ? 1 : 3;
    ATTRIB_POINTERS.normal.stride = stride;
    ATTRIB_POINTERS.normal.type = type;
	ATTRIB_POINTERS.normal_func = calcReadNormalFunc();

    _glRecalcFastPath();
}


void _glInitAttributePointers() {
    TRACE();

    ATTRIB_POINTERS.vertex.ptr = NULL;
    ATTRIB_POINTERS.vertex.stride = 0;
    ATTRIB_POINTERS.vertex.type = GL_FLOAT;
    ATTRIB_POINTERS.vertex.size = 4;
	ATTRIB_POINTERS.vertex_func = calcReadPositionFunc();

    ATTRIB_POINTERS.colour.ptr = NULL;
    ATTRIB_POINTERS.colour.stride = 0;
    ATTRIB_POINTERS.colour.type = GL_FLOAT;
    ATTRIB_POINTERS.colour.size = 4;
	ATTRIB_POINTERS.colour_func = calcReadDiffuseFunc();

    ATTRIB_POINTERS.uv.ptr = NULL;
    ATTRIB_POINTERS.uv.stride = 0;
    ATTRIB_POINTERS.uv.type = GL_FLOAT;
    ATTRIB_POINTERS.uv.size = 4;
	ATTRIB_POINTERS.uv_func = calcReadUVFunc();

    ATTRIB_POINTERS.st.ptr = NULL;
    ATTRIB_POINTERS.st.stride = 0;
    ATTRIB_POINTERS.st.type = GL_FLOAT;
    ATTRIB_POINTERS.st.size = 4;
	ATTRIB_POINTERS.st_func = calcReadSTFunc();

    ATTRIB_POINTERS.normal.ptr = NULL;
    ATTRIB_POINTERS.normal.stride = 0;
    ATTRIB_POINTERS.normal.type = GL_FLOAT;
    ATTRIB_POINTERS.normal.size = 3;
	ATTRIB_POINTERS.normal_func = calcReadNormalFunc();
}
