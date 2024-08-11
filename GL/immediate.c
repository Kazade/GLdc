/*
 * This implements immediate mode over the top of glDrawArrays
 * current problems:
 *
 * 1. Calling glNormal(); glVertex(); glVertex(); glVertex(); will break.
 * 2. Mixing with glXPointer stuff will break badly
 * 3. This is entirely untested.
 */

#include <string.h>
#include <stdio.h>

#include "private.h"

extern inline GLuint _glRecalcFastPath();

GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static GLfloat __attribute__((aligned(32))) NORMAL[3] = {0.0f, 0.0f, 1.0f};
static GLubyte __attribute__((aligned(32))) COLOR[4] = {255, 255, 255, 255}; /* ARGB order for speed */
static GLfloat __attribute__((aligned(32))) UV_COORD[2] = {0.0f, 0.0f};
static GLfloat __attribute__((aligned(32))) ST_COORD[2] = {0.0f, 0.0f};

static AlignedVector VERTICES;
static AttribPointerList IM_ATTRIBS;

/* We store the list of attributes that have been "enabled" by a call to
  glColor, glNormal, glTexCoord etc. otherwise we already have defaults that
  can be applied faster */
static GLuint IM_ENABLED_VERTEX_ATTRIBUTES = 0;

typedef struct __attribute__((aligned(32))) {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLfloat s;
    GLfloat t;
    GLubyte bgra[4];
    GLfloat nx;
    GLfloat ny;
    GLfloat nz;
    GLuint padding[5];
} IMVertex;


void _glInitImmediateMode(GLuint initial_size) {
    aligned_vector_init(&VERTICES, sizeof(IMVertex));
    aligned_vector_reserve(&VERTICES, initial_size);

    IM_ATTRIBS.vertex.ptr = aligned_vector_front(&VERTICES);
    IM_ATTRIBS.vertex.size = 3;
    IM_ATTRIBS.vertex.type = GL_FLOAT;
    IM_ATTRIBS.vertex.stride = sizeof(IMVertex);

    IM_ATTRIBS.uv.ptr = IM_ATTRIBS.vertex.ptr + (sizeof(GLfloat) * 3);
    IM_ATTRIBS.uv.stride = sizeof(IMVertex);
    IM_ATTRIBS.uv.type = GL_FLOAT;
    IM_ATTRIBS.uv.size = 2;

    IM_ATTRIBS.st.ptr = IM_ATTRIBS.vertex.ptr + (sizeof(GLfloat) * 5);
    IM_ATTRIBS.st.stride = sizeof(IMVertex);
    IM_ATTRIBS.st.type = GL_FLOAT;
    IM_ATTRIBS.st.size = 2;

    IM_ATTRIBS.colour.ptr = IM_ATTRIBS.vertex.ptr + (sizeof(GLfloat) * 7);
    IM_ATTRIBS.colour.size = GL_BGRA;  /* Flipped color order */
    IM_ATTRIBS.colour.type = GL_UNSIGNED_BYTE;
    IM_ATTRIBS.colour.stride = sizeof(IMVertex);

    IM_ATTRIBS.normal.ptr = IM_ATTRIBS.vertex.ptr + (sizeof(GLfloat) * 7) + sizeof(uint32_t);
    IM_ATTRIBS.normal.stride = sizeof(IMVertex);
    IM_ATTRIBS.normal.type = GL_FLOAT;
    IM_ATTRIBS.normal.size = 3;
}

void APIENTRY glBegin(GLenum mode) {
    if(IMMEDIATE_MODE_ACTIVE) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    IMMEDIATE_MODE_ACTIVE = GL_TRUE;
    ACTIVE_POLYGON_MODE = mode;
}

void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = (GLubyte)(a * 255.0f);
    COLOR[R8IDX] = (GLubyte)(r * 255.0f);
    COLOR[G8IDX] = (GLubyte)(g * 255.0f);
    COLOR[B8IDX] = (GLubyte)(b * 255.0f);
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = a;
    COLOR[R8IDX] = r;
    COLOR[G8IDX] = g;
    COLOR[B8IDX] = b;
}

void APIENTRY glColor4ubv(const GLubyte *v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = v[3];
    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
}

void APIENTRY glColor4fv(const GLfloat* v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[B8IDX] = (GLubyte)(v[2] * 255);
    COLOR[G8IDX] = (GLubyte)(v[1] * 255);
    COLOR[R8IDX] = (GLubyte)(v[0] * 255);
    COLOR[A8IDX] = (GLubyte)(v[3] * 255);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[B8IDX] = (GLubyte)(b * 255.0f);
    COLOR[G8IDX] = (GLubyte)(g * 255.0f);
    COLOR[R8IDX] = (GLubyte)(r * 255.0f);
    COLOR[A8IDX] = 255;
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = red;
    COLOR[G8IDX] = green;
    COLOR[B8IDX] = blue;
}

void APIENTRY glColor3ubv(const GLubyte *v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
}

void APIENTRY glColor3fv(const GLfloat* v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = (GLubyte)(v[0] * 255);
    COLOR[G8IDX] = (GLubyte)(v[1] * 255);
    COLOR[B8IDX] = (GLubyte)(v[2] * 255);
}

typedef union punned {
    GLubyte*   byte;
    GLfloat*   flt;
    uint32_t*  u32;
    void*      vptr;
    uintptr_t  uptr;
} punned_t;

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= VERTEX_ENABLED_FLAG;

    IMVertex* vert = aligned_vector_extend(&VERTICES, 1);

    punned_t dest = { .flt = &vert->x };
    *(dest.flt++) = x;
    *(dest.flt++) = y;
    *(dest.flt++) = z;
    *(dest.flt++) = UV_COORD[0];
    *(dest.flt++) = UV_COORD[1];
    *(dest.flt++) = ST_COORD[0];
    *(dest.flt++) = ST_COORD[1];
    *(dest.u32++) = *((uint32_t*)(void*) COLOR);
    *(dest.flt++) = NORMAL[0];
    *(dest.flt++) = NORMAL[1];
    *(dest.flt++) = NORMAL[2];
}

void APIENTRY glVertex3fv(const GLfloat* v) {
    glVertex3f(v[0], v[1], v[2]);
}

void APIENTRY glVertex2f(GLfloat x, GLfloat y) {
    glVertex3f(x, y, 0.0f);
}

void APIENTRY glVertex2fv(const GLfloat* v) {
    glVertex2f(v[0], v[1]);
}

void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _GL_UNUSED(w);
    glVertex3f(x, y, z);
}

void APIENTRY glVertex4fv(const GLfloat* v) {
    glVertex4f(v[0], v[1], v[2], v[3]);
}

void APIENTRY glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) {
    if(target == GL_TEXTURE0) {
        IM_ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG;
        UV_COORD[0] = s;
        UV_COORD[1] = t;
    } else if(target == GL_TEXTURE1) {
        IM_ENABLED_VERTEX_ATTRIBUTES |= ST_ENABLED_FLAG;
        ST_COORD[0] = s;
        ST_COORD[1] = t;
    } else {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }
}

void APIENTRY glTexCoord1f(GLfloat u) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG;
    UV_COORD[0] = u;
    UV_COORD[1] = 0.0f;
}

void APIENTRY glTexCoord1fv(const GLfloat* v) {
    glTexCoord1f(v[0]);
}

void APIENTRY glTexCoord2f(GLfloat u, GLfloat v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG;
    UV_COORD[0] = u;
    UV_COORD[1] = v;
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    glTexCoord2f(v[0], v[1]);
}

void APIENTRY glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= NORMAL_ENABLED_FLAG;
    NORMAL[0] = x;
    NORMAL[1] = y;
    NORMAL[2] = z;
}

void APIENTRY glNormal3fv(const GLfloat* v) {
    glNormal3f(v[0], v[1], v[2]);
}

void APIENTRY glEnd() {
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* Resizing could've invalidated the pointers */
    IM_ATTRIBS.vertex.ptr = VERTICES.data;
    IM_ATTRIBS.uv.ptr     = IM_ATTRIBS.vertex.ptr + 12;
    IM_ATTRIBS.st.ptr     = IM_ATTRIBS.uv.ptr + 8;
    IM_ATTRIBS.colour.ptr = IM_ATTRIBS.st.ptr + 8;
    IM_ATTRIBS.normal.ptr = IM_ATTRIBS.colour.ptr + 4;

    GLuint* attrs = &ENABLED_VERTEX_ATTRIBUTES;

    /* Redirect attrib pointers */
    AttribPointerList stashed_attrib_pointers = ATTRIB_POINTERS;
    ATTRIB_POINTERS = IM_ATTRIBS;

    GLuint prevAttrs = *attrs;

    *attrs = IM_ENABLED_VERTEX_ATTRIBUTES;

    /* Store the fast path enabled setting so we can restore it
     * after drawing */
    const GLboolean fp_was_enabled = FAST_PATH_ENABLED;

#ifndef NDEBUG
    // Immediate mode should always activate the fast path
    GLuint fastPathEnabled = _glRecalcFastPath();
    gl_assert(fastPathEnabled);
#else
    /* If we're not debugging, set to true - we assume we haven't broken it! */
    FAST_PATH_ENABLED = GL_TRUE;
#endif

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, aligned_vector_header(&VERTICES)->size);

    ATTRIB_POINTERS = stashed_attrib_pointers;

    *attrs = prevAttrs;

    aligned_vector_clear(&VERTICES);

    FAST_PATH_ENABLED = fp_was_enabled;
}

void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    glBegin(GL_QUADS);
        glVertex2f(x1, y1);
        glVertex2f(x2, y1);
        glVertex2f(x2, y2);
        glVertex2f(x1, y2);
    glEnd();
}

void APIENTRY glRectfv(const GLfloat *v1, const GLfloat *v2) {
    glBegin(GL_QUADS);
        glVertex2f(v1[0], v1[1]);
        glVertex2f(v2[0], v1[1]);
        glVertex2f(v2[0], v2[1]);
        glVertex2f(v1[0], v2[1]);
    glEnd();
}

void APIENTRY glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
    return glRectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2);
}

void APIENTRY glRectiv(const GLint *v1, const GLint *v2) {
    return glRectfv((const GLfloat *)v1, (const GLfloat *)v2);
}
