#include "../include/gl.h"
#include "private.h"


typedef struct {
    void* ptr;
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
    VERTEX_POINTER.ptr = NULL;
    VERTEX_POINTER.stride = 0;
    VERTEX_POINTER.type = GL_FLOAT;
    VERTEX_POINTER.size = 4;
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
    default: return sizeof(GLfloat);
    }
}

static void transformVertex(const GLfloat* src) {
    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    __x = src[0];
    __y = src[1];
    __z = src[2];

    mat_trans_fv12()

    src[0] = __x;
    src[1] = __y;
    src[2] = __z;
}

static void _parseFloats(GLfloat* out, const GLubyte* in, GLint size, GLenum type) {
    switch(type) {
    case GL_SHORT: {
        GLshort* inp = (GLshort*) in;
        for(GLubyte i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_INT: {
        GLint* inp = (GLint*) in;
        for(GLubyte i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_FLOAT:
    case GL_DOUBLE:  /* Double == Float */
    default:
        for(GLubyte i = 0; i < size; ++i) out[i] = ((GLfloat*) in)[i];
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

static void submitVertices(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

    const GLbyte elements = (mode == GL_QUADS) ? 4 : (mode == GL_TRIANGLES) ? 3 : (mode == GL_LINES) ? 2 : count;

    pvr_vertex_t vertex;
    vertex.u = vertex.v = 0.0f;
    vertex.argb = 0xFFFFFFFF;

    static GLfloat vparts[4];

    GLubyte vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    GLubyte* vptr = VERTEX_POINTER.ptr;

    for(GLuint i = 0; i < count; ++i) {
        GLshort idx = i;
        if(indices) {
            _parseIndex(&idx, indices[byte_size(type) * i], type);
        }

        _parseFloats(vparts, vptr + (idx * vstride), VERTEX_POINTER.size, VERTEX_POINTER.type);

        vertex.x = vparts[0];
        vertex.y = vparts[1];
        vertex.z = vparts[2];

        transformVertex(&vertex.x);
    }
}

GLAPI void APIENTRY glDrawElementsExp(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    submitVertices(mode, count, type, indices);
}

GLAPI void APIENTRY glDrawArraysExp(GLenum mode, GLint first, GLsizei count) {
    submitVertices(mode, count, type, NULL);
}

GLAPI void APIENTRY glEnableClientStateExp(GLenum cap) {
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

GLAPI void APIENTRY glDisableClientStateExp(GLenum cap) {
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

GLAPI void APIENTRY glClientActiveTextureExp(GLenum texture) {
    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1) ? 1 : 0;
}

GLAPI void APIENTRY glTexcoordPointerExp(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    AttribPointer* texpointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &UV_POINTER : &ST_POINTER;

    texpointer->ptr = pointer;
    texpointer->stride = stride;
    texpointer->type = type;
    texpointer->size = size;
}

GLAPI void APIENTRY glVertexPointerExp(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    VERTEX_POINTER.ptr = pointer;
    VERTEX_POINTER.stride = stride;
    VERTEX_POINTER.type = type;
    VERTEX_POINTER.size = size;
}

GLAPI void APIENTRY glColorPointerExp(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    DIFFUSE_POINTER.ptr = pointer;
    DIFFUSE_POINTER.stride = stride;
    DIFFUSE_POINTER.type = type;
    DIFFUSE_POINTER.size = size;
}

GLAPI void APIENTRY glNormalPointerExp(GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    NORMAL_POINTER.ptr = pointer;
    NORMAL_POINTER.stride = stride;
    NORMAL_POINTER.type = type;
    NORMAL_POINTER.size = 3;
}
