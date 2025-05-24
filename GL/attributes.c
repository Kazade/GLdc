#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"

AttribPointerList ATTRIB_LIST;
static const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;

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

// Used to avoid checking and updating attribute related state unless necessary
GL_FORCE_INLINE GLboolean _glStateUnchanged(AttribPointer* p, GLint size, GLenum type, GLsizei stride) {
    return (p->size == size && p->type == type && p->stride == stride);
}

GLuint* _glGetEnabledAttributes(void) {
    return &ATTRIB_LIST.enabled;
}

static void _readPosition3f3f(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    const float* input = (const float*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = input[2];
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition3d3f(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    const double* input = (const double*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = input[2];
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition3ub3f(const GLubyte* input, GLubyte* out) {
    Vertex* it = (Vertex*)out;

    float x = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    float y = input[1] * ONE_OVER_TWO_FIVE_FIVE;
    float z = input[2] * ONE_OVER_TWO_FIVE_FIVE;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition3us3f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = input[2];
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition3ui3f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = input[2];
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition2f3f(const GLubyte* in, GLubyte* out) {
    const float* input = (const float*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = 0.0f;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition2d3f(const GLubyte* in, GLubyte* out) {
    const double* input = (const double*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = 0.0f;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition2ub3f(const GLubyte* input, GLubyte* out) {
    Vertex* it = (Vertex*) out;

    float x = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    float y = input[1] * ONE_OVER_TWO_FIVE_FIVE;
    float z = 0.0f;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition2us3f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    Vertex* it = (Vertex*) out;

    float x = input[0];
    float y = input[1];
    float z = 0.0f;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static void _readPosition2ui3f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    Vertex* it = (Vertex*)out;

    float x = input[0];
    float y = input[1];
    float z = 0.0f;
    float w = 1.0f;
    TransformVertex(x, y, z, w, it->xyz, &it->w);
}

static ReadAttributeFunc calcReadPositionFunc(void) {
    switch(ATTRIB_LIST.vertex.type) {
        case GL_DOUBLE:
            return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3d3f:
                    _readPosition2d3f;
        default:
        case GL_FLOAT:
            return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3f3f:
                    _readPosition2f3f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3ub3f:
                    _readPosition2ub3f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3us3f:
                    _readPosition2us3f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3ui3f:
                    _readPosition2ui3f;
    }
}

static void _fillWhiteARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    _GL_UNUSED(input);
    *((uint32_t*) output) = ~0;
}

static void _readColour4ubARGB(const GLubyte* input, GLubyte* output) {
    output[R8IDX] = input[0];
    output[G8IDX] = input[1];
    output[B8IDX] = input[2];
    output[A8IDX] = input[3];
}

static void _readColour3ubARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    output[R8IDX] = input[0];
    output[G8IDX] = input[1];
    output[B8IDX] = input[2];
    output[A8IDX] = 255;
}

#define DEF_READ_COLOUR_4_ARGB_FP(prefix, intype) \
    static void _readColour##prefix##ARGB(const GLubyte* __restrict in, GLubyte* __restrict out) { \
        const intype* input = (const intype*) in; \
        out[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255); \
        out[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255); \
        out[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255); \
        out[A8IDX] = (GLubyte) clamp(input[3] * 255.0f, 0, 255); \
    }

#define DEF_READ_COLOUR_3_ARGB_FP(prefix, intype) \
    static void _readColour##prefix##ARGB(const GLubyte* __restrict in, GLubyte* __restrict out) { \
        const intype* input = (const intype*) in; \
        out[R8IDX] = (GLubyte) clamp(input[0] * 255.0f, 0, 255); \
        out[G8IDX] = (GLubyte) clamp(input[1] * 255.0f, 0, 255); \
        out[B8IDX] = (GLubyte) clamp(input[2] * 255.0f, 0, 255); \
        out[A8IDX] = 255; \
    }

DEF_READ_COLOUR_4_ARGB_FP(4f, float)

DEF_READ_COLOUR_3_ARGB_FP(3f, float)



static void _readColour4dARGB(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    const double* input = (const double*) in;
    
    // Convert to float first  
    float r = (float)input[0];
    float g = (float)input[1];
    float b = (float)input[2]; 
    float a = (float)input[3];
    
    out[R8IDX] = (GLubyte) clamp(r * 255.0f, 0, 255);
    out[G8IDX] = (GLubyte) clamp(g * 255.0f, 0, 255);
    out[B8IDX] = (GLubyte) clamp(b * 255.0f, 0, 255);
    out[A8IDX] = (GLubyte) clamp(a * 255.0f, 0, 255);
}

static void _readColour3dARGB(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    const double* input = (const double*) in;
    
    // Convert to float first  
    float r = (float)input[0];
    float g = (float)input[1];
    float b = (float)input[2];
    
    out[R8IDX] = (GLubyte) clamp(r * 255.0f, 0, 255);
    out[G8IDX] = (GLubyte) clamp(g * 255.0f, 0, 255);
    out[B8IDX] = (GLubyte) clamp(b * 255.0f, 0, 255);
    out[A8IDX] = 255;
}



static void _readColour4ubRevARGB(const GLubyte* __restrict__ input, GLubyte* __restrict__ output) {
    argbcpy(output, input);
}

static void _readColour4fRevARGB(const GLubyte* __restrict__ in, GLubyte* __restrict__ output) {
    const float* input = (const float*) in;

    output[0] = (GLubyte) clamp(input[0] * 255.0f, 0, 255);
    output[1] = (GLubyte) clamp(input[1] * 255.0f, 0, 255);
    output[2] = (GLubyte) clamp(input[2] * 255.0f, 0, 255);
    output[3] = (GLubyte) clamp(input[3] * 255.0f, 0, 255);
}

static void _readColour4dRevARGB(const GLubyte* __restrict__ in, GLubyte* __restrict__ output) {
    const double* input = (const double*) in;

    // Convert to float first  
    float r = (float)input[0];
    float g = (float)input[1];
    float b = (float)input[2];
    float a = (float)input[3];

    output[0] = (GLubyte) clamp(r * 255.0f, 0, 255);
    output[1] = (GLubyte) clamp(g * 255.0f, 0, 255);
    output[2] = (GLubyte) clamp(b * 255.0f, 0, 255);
    output[3] = (GLubyte) clamp(a * 255.0f, 0, 255);
}

#define DEF_READ_COLOUR_N_ARGB_INT(prefix, intype, max, alpha, i0, i1, i2, i3) \
    static void _readColour##prefix##ARGB(const GLubyte* __restrict in, GLubyte* __restrict out) { \
        const intype* input = (const intype*) in; \
        out[i0] = (GLubyte) clamp((float)input[0] / (float)max * 255.0f, 0, 255); \
        out[i1] = (GLubyte) clamp((float)input[1] / (float)max * 255.0f, 0, 255); \
        out[i2] = (GLubyte) clamp((float)input[2] / (float)max * 255.0f, 0, 255); \
        out[i3] = alpha; \
    }

#define DEF_READ_COLOUR_3_ARGB_INT(prefix, intype, max) \
    DEF_READ_COLOUR_N_ARGB_INT(prefix, intype, max, 255, R8IDX, G8IDX, B8IDX, A8IDX)

#define DEF_READ_COLOUR_4_ARGB_INT(prefix, intype, max) \
    DEF_READ_COLOUR_N_ARGB_INT(prefix, intype, max,  \
        ((GLubyte)clamp((float)input[3] / (float)max * 255.0f, 0, 255)), \
        R8IDX, G8IDX, B8IDX, A8IDX)

#define DEF_READ_COLOUR_4_REV_ARGB_INT(prefix, intype, max) \
    DEF_READ_COLOUR_N_ARGB_INT(prefix##Rev, intype, max,  \
        ((GLubyte)clamp((float)input[3] / (float)max * 255.0f, 0, 255)), \
        0, 1, 2, 3)

DEF_READ_COLOUR_3_ARGB_INT(3us, GLushort, UINT16_MAX)
DEF_READ_COLOUR_3_ARGB_INT(3ui, GLuint, UINT32_MAX)

DEF_READ_COLOUR_4_ARGB_INT(4us, GLushort, UINT16_MAX)
DEF_READ_COLOUR_4_ARGB_INT(4ui, GLuint, UINT32_MAX)

DEF_READ_COLOUR_4_REV_ARGB_INT(4us, GLushort, UINT16_MAX)
DEF_READ_COLOUR_4_REV_ARGB_INT(4ui, GLuint, UINT32_MAX)

static ReadAttributeFunc calcReadDiffuseFunc(void) {
    if((ATTRIB_LIST.enabled & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) {
        /* Just fill the whole thing white if the attribute is disabled */
        return _fillWhiteARGB;
    }

    switch(ATTRIB_LIST.colour.type) {
        case GL_DOUBLE:
            return (ATTRIB_LIST.colour.size == 3) ? _readColour3dARGB:
                   (ATTRIB_LIST.colour.size == 4) ? _readColour4dARGB:
                    _readColour4dRevARGB;
        default:
        case GL_FLOAT:
            return (ATTRIB_LIST.colour.size == 3) ? _readColour3fARGB:
                   (ATTRIB_LIST.colour.size == 4) ? _readColour4fARGB:
                    _readColour4fRevARGB;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return (ATTRIB_LIST.colour.size == 3) ? _readColour3ubARGB:
                   (ATTRIB_LIST.colour.size == 4) ? _readColour4ubARGB:
                    _readColour4ubRevARGB;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return (ATTRIB_LIST.colour.size == 3) ? _readColour3usARGB:
                   (ATTRIB_LIST.colour.size == 4) ? _readColour4usARGB:
                    _readColour4usRevARGB;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return (ATTRIB_LIST.colour.size == 3) ? _readColour3uiARGB:
                   (ATTRIB_LIST.colour.size == 4) ? _readColour4uiARGB:
                    _readColour4uiRevARGB;
    }
}


static void _fillZero2f(const GLubyte* __restrict__ input, GLubyte* __restrict__ out) {
    _GL_UNUSED(input);
    //memset(out, 0, sizeof(float) * 2);
    // memset does 8 individual byte writes - faster to manually write as uint32
    uint32_t* dst = (uint32_t*)out;
    dst[0] = 0;
    dst[1] = 0;
}

static void _readTexcoord2f2f(const GLubyte* in, GLubyte* out) {
    vec2cpy(out, in);
}

static void _readTexcoord2d2f(const GLubyte* in, GLubyte* out) {
    ((float*)out)[0] = ((const double*)in)[0];
    ((float*)out)[1] = ((const double*)in)[1];
}

static void _readTexcoord2ub2f(const GLubyte* input, GLubyte* out) {
    float* output = (float*) out;

    output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
}

static void _readTexcoord2us2f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    float* output = (float*) out;

    output[0] = (float)input[0] / SHRT_MAX;
    output[1] = (float)input[1] / SHRT_MAX;
}

static void _readTexcoord2ui2f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
}

static ReadAttributeFunc calcReadUVFunc(void) {
    if((ATTRIB_LIST.enabled & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) {
        return _fillZero2f;
    }

    switch(ATTRIB_LIST.uv.type) {
        case GL_DOUBLE:
            return _readTexcoord2d2f;
        default:
        case GL_FLOAT:
            return _readTexcoord2f2f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readTexcoord2ub2f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readTexcoord2us2f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readTexcoord2ui2f;
    }
}

static ReadAttributeFunc calcReadSTFunc(void) {
    if((ATTRIB_LIST.enabled & ST_ENABLED_FLAG) != ST_ENABLED_FLAG) {
        return _fillZero2f;
    }

    switch(ATTRIB_LIST.st.type) {
        case GL_DOUBLE:
            return _readTexcoord2d2f;
        default:
        case GL_FLOAT:
            return _readTexcoord2f2f;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readTexcoord2ub2f;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readTexcoord2us2f;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readTexcoord2ui2f;
    }
}


static void _fillWithNegZVE(const GLubyte* __restrict__ input, GLubyte* __restrict__ out) {
    _GL_UNUSED(input);
    typedef struct { float x, y, z; } V;

    static const V NegZ = {0.0f, 0.0f, -1.0f};

    *((V*) out) = NegZ;
}

static void _readNormal3f3f(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    vec3cpy(out, in);
}

static void _readNormal3d3f(const GLubyte* __restrict__ in, GLubyte* __restrict__ out) {
    ((float*)out)[0] = ((const double*)in)[0];
    ((float*)out)[1] = ((const double*)in)[1];
    ((float*)out)[2] = ((const double*)in)[2];
}

static void _readNormal3ub3f(const GLubyte* input, GLubyte* out) {
    float* output = (float*) out;

    output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
    output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
    output[2] = input[2] * ONE_OVER_TWO_FIVE_FIVE;
}

static void _readNormal3us3f(const GLubyte* in, GLubyte* out) {
    const GLushort* input = (const GLushort*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}

static void _readNormal3ui3f(const GLubyte* in, GLubyte* out) {
    const GLuint* input = (const GLuint*) in;
    float* output = (float*) out;

    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}

// 10:10:10:2REV format
static void _readNormal1i3f(const GLubyte* in, GLubyte* out) {
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

static ReadAttributeFunc calcReadNormalFunc(void) {
    if((ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG) != NORMAL_ENABLED_FLAG) {
        return _fillWithNegZVE;
    }

    switch(ATTRIB_LIST.normal.type) {
        case GL_DOUBLE:
            return _readNormal3d3f;
        case GL_FLOAT:
        default:
            return _readNormal3f3f;
        break;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return _readNormal3ub3f;
        break;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return _readNormal3us3f;
        break;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return _readNormal3ui3f;
        break;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return _readNormal1i3f;
        break;
    }
}


void APIENTRY glEnableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ATTRIB_LIST.enabled |= VERTEX_ENABLED_FLAG;
	    ATTRIB_LIST.dirty   |= VERTEX_ENABLED_FLAG;
        break;
    case GL_COLOR_ARRAY:
        ATTRIB_LIST.enabled |= DIFFUSE_ENABLED_FLAG;
	    ATTRIB_LIST.dirty   |= DIFFUSE_ENABLED_FLAG;
        break;
    case GL_NORMAL_ARRAY:
        ATTRIB_LIST.enabled |= NORMAL_ENABLED_FLAG;
        ATTRIB_LIST.dirty   |= NORMAL_ENABLED_FLAG;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ATTRIB_LIST.enabled |= ST_ENABLED_FLAG):
            (ATTRIB_LIST.enabled |= UV_ENABLED_FLAG);

        (ACTIVE_CLIENT_TEXTURE) ?
            (ATTRIB_LIST.dirty   |= ST_ENABLED_FLAG):
            (ATTRIB_LIST.dirty   |= UV_ENABLED_FLAG);
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glDisableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ATTRIB_LIST.enabled &= ~VERTEX_ENABLED_FLAG;
	    ATTRIB_LIST.dirty   |=  VERTEX_ENABLED_FLAG;
        break;
    case GL_COLOR_ARRAY:
        ATTRIB_LIST.enabled &= ~DIFFUSE_ENABLED_FLAG;
	    ATTRIB_LIST.dirty   |=  DIFFUSE_ENABLED_FLAG;
        break;
    case GL_NORMAL_ARRAY:
        ATTRIB_LIST.enabled &= ~NORMAL_ENABLED_FLAG;
	    ATTRIB_LIST.dirty   |=  NORMAL_ENABLED_FLAG;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ATTRIB_LIST.enabled &= ~ST_ENABLED_FLAG):
            (ATTRIB_LIST.enabled &= ~UV_ENABLED_FLAG);

        (ACTIVE_CLIENT_TEXTURE) ?
            (ATTRIB_LIST.dirty   |=  ST_ENABLED_FLAG):
            (ATTRIB_LIST.dirty   |=  UV_ENABLED_FLAG);
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}


void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    TRACE();

    stride = (stride) ? stride : size * byte_size(type);
    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &ATTRIB_LIST.uv : &ATTRIB_LIST.st;
    tointer->ptr = pointer;

    if(_glStateUnchanged(tointer, size, type, stride)) return;

    if(size < 1 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;

	(ACTIVE_CLIENT_TEXTURE) ?
        (ATTRIB_LIST.dirty |= ST_ENABLED_FLAG):
        (ATTRIB_LIST.dirty |= UV_ENABLED_FLAG);
}

void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer) {
    TRACE();

    stride = (stride) ? stride : (size * byte_size(type));
    ATTRIB_LIST.vertex.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_LIST.vertex, size, type, stride)) return;

    if(size < 2 || size > 4) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    ATTRIB_LIST.vertex.stride = stride;
    ATTRIB_LIST.vertex.type = type;
    ATTRIB_LIST.vertex.size = size;

	ATTRIB_LIST.dirty |= VERTEX_ENABLED_FLAG;
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    stride = (stride) ? stride : ((size == GL_BGRA) ? 4 : size) * byte_size(type);
    ATTRIB_LIST.colour.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_LIST.colour, size, type, stride)) return;

    if(size != 3 && size != 4 && size != GL_BGRA) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    ATTRIB_LIST.colour.type = type;
    ATTRIB_LIST.colour.size = size;
    ATTRIB_LIST.colour.stride = stride;

	ATTRIB_LIST.dirty |= DIFFUSE_ENABLED_FLAG;
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

    stride = (stride) ? stride : ATTRIB_LIST.normal.size * byte_size(type);
    ATTRIB_LIST.normal.ptr = pointer;

    if(_glStateUnchanged(&ATTRIB_LIST.normal, 3, type, stride)) return;

    if(_glCheckValidEnum(type, validTypes, __func__) != 0) {
        return;
    }

    ATTRIB_LIST.normal.size = (type == GL_UNSIGNED_INT_2_10_10_10_REV) ? 1 : 3;
    ATTRIB_LIST.normal.stride = stride;
    ATTRIB_LIST.normal.type = type;

	ATTRIB_LIST.dirty |= NORMAL_ENABLED_FLAG;
}


void _glInitAttributePointers(void) {
    TRACE();
    ATTRIB_LIST.dirty = ~0; // all attributes dirty

    glVertexPointer(3, GL_FLOAT, 0, NULL);
    glTexCoordPointer(2, GL_FLOAT, 0, NULL);
    glColorPointer(4, GL_FLOAT, 0, NULL);
    glNormalPointer(GL_FLOAT, 0, NULL);
}

GL_FORCE_INLINE GLuint _glIsVertexDataFastPathCompatible(void) {
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

    if((ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG)) {
        if(ATTRIB_LIST.vertex.size != 3 || ATTRIB_LIST.vertex.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ATTRIB_LIST.enabled & UV_ENABLED_FLAG)) {
        if(ATTRIB_LIST.uv.size != 2 || ATTRIB_LIST.uv.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ATTRIB_LIST.enabled & DIFFUSE_ENABLED_FLAG)) {
        /* FIXME: Shouldn't this be a reversed format? */
        if(ATTRIB_LIST.colour.size != GL_BGRA || ATTRIB_LIST.colour.type != GL_UNSIGNED_BYTE) {
            return GL_FALSE;
        }
    }

    if((ATTRIB_LIST.enabled & ST_ENABLED_FLAG)) {
        if(ATTRIB_LIST.st.size != 2 || ATTRIB_LIST.st.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    if((ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG)) {
        if(ATTRIB_LIST.normal.size != 3 || ATTRIB_LIST.normal.type != GL_FLOAT) {
            return GL_FALSE;
        }
    }

    return GL_TRUE;
}

void _glUpdateAttributes(void) {
    if(ATTRIB_LIST.dirty & VERTEX_ENABLED_FLAG) {
        ATTRIB_LIST.vertex_func = calcReadPositionFunc();
    }

    if(ATTRIB_LIST.dirty & UV_ENABLED_FLAG) {
        ATTRIB_LIST.uv_func = calcReadUVFunc();
    }

    if(ATTRIB_LIST.dirty & DIFFUSE_ENABLED_FLAG) {
        ATTRIB_LIST.colour_func = calcReadDiffuseFunc();
    }

    if(ATTRIB_LIST.dirty & ST_ENABLED_FLAG) {
        ATTRIB_LIST.st_func = calcReadSTFunc();
    }

    if(ATTRIB_LIST.dirty & NORMAL_ENABLED_FLAG) {
        ATTRIB_LIST.normal_func = calcReadNormalFunc();
    }

    ATTRIB_LIST.fast_path = _glIsVertexDataFastPathCompatible();
    ATTRIB_LIST.dirty     = 0;
}