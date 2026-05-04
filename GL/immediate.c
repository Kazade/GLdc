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

#include "state.h"
#include "private.h"

GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AttribPointerList IM_ATTRIBS;

typedef struct __attribute__((aligned(32))) {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLfloat s;
    GLfloat t;
    GLfloat argb[4];
    GLfloat nx;
    GLfloat ny;
    GLfloat nz;
    GLuint padding[2];
} IMVertex;


void _glInitImmediateMode(GLuint initial_size) {
    aligned_vector_init(&VERTICES, sizeof(IMVertex));
    aligned_vector_reserve(&VERTICES, initial_size);
    IM_ATTRIBS.fast_path = GL_TRUE;

    IM_ATTRIBS.vertex.ptr = aligned_vector_front(&VERTICES);
    IM_ATTRIBS.vertex.size = 3;
    IM_ATTRIBS.vertex.type = GL_FLOAT;
    IM_ATTRIBS.vertex.stride = sizeof(IMVertex);

    IM_ATTRIBS.uv.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, u);
    IM_ATTRIBS.uv.stride = sizeof(IMVertex);
    IM_ATTRIBS.uv.type = GL_FLOAT;
    IM_ATTRIBS.uv.size = 2;

    IM_ATTRIBS.st.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, s);
    IM_ATTRIBS.st.stride = sizeof(IMVertex);
    IM_ATTRIBS.st.type = GL_FLOAT;
    IM_ATTRIBS.st.size = 2;

    IM_ATTRIBS.colour.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, argb);
    IM_ATTRIBS.colour.size = 4;
    IM_ATTRIBS.colour.type = GL_FLOAT;
    IM_ATTRIBS.colour.stride = sizeof(IMVertex);

    IM_ATTRIBS.normal.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, nx);
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
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    float* COLOR = _glCurrentColor();

    COLOR[A8IDX] = a;
    COLOR[R8IDX] = r;
    COLOR[G8IDX] = g;
    COLOR[B8IDX] = b;
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    const float m = 1.0f / 255.0f;

    float* COLOR = _glCurrentColor();
    COLOR[A8IDX] = ((float)a) * m;
    COLOR[R8IDX] = ((float)r) * m;
    COLOR[G8IDX] = ((float)g) * m;
    COLOR[B8IDX] = ((float)b) * m;
}

void APIENTRY glColor4ubv(const GLubyte *v) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    const float m = 1.0f / 255.0f;

    float* COLOR = _glCurrentColor();

    COLOR[R8IDX] = ((float) v[0]) * m;
    COLOR[G8IDX] = ((float) v[1]) * m;
    COLOR[B8IDX] = ((float) v[2]) * m;
    COLOR[A8IDX] = ((float) v[3]) * m;
}

void APIENTRY glColor4fv(const GLfloat* v) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    float* COLOR = _glCurrentColor();

    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
    COLOR[A8IDX] = v[3];
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    float* COLOR = _glCurrentColor();

    COLOR[B8IDX] = b;
    COLOR[G8IDX] = g;
    COLOR[R8IDX] = r;
    COLOR[A8IDX] = 1.0f;
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    const float m = 1.0f / 255.0f;

    float* COLOR = _glCurrentColor();

    COLOR[A8IDX] = 1.0f;
    COLOR[R8IDX] = ((float) red) * m;
    COLOR[G8IDX] = ((float) green) * m;
    COLOR[B8IDX] = ((float) blue) * m;
}

void APIENTRY glColor3ubv(const GLubyte *v) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    const float m = 1.0f / 255.0f;

    float* COLOR = _glCurrentColor();

    COLOR[A8IDX] = 1.0f;
    COLOR[R8IDX] = ((float) v[0]) * m;
    COLOR[G8IDX] = ((float) v[1]) * m;
    COLOR[B8IDX] = ((float) v[2]) * m;
}

void APIENTRY glColor3fv(const GLfloat* v) {
    IM_ATTRIBS.enabled |= COLOR_ENABLED_FLAG;

    float* COLOR = _glCurrentColor();

    COLOR[A8IDX] = 1.0f;
    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
}

typedef union punned {
    GLubyte*   byte;
    GLfloat*   flt;
    uint32_t*  u32;
    void*      vptr;
    uintptr_t  uptr;
} punned_t;

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    IM_ATTRIBS.enabled |= VERTEX_ENABLED_FLAG;

    IMVertex* vert = aligned_vector_extend(&VERTICES, 1);

    float* COLOR = _glCurrentColor();
    float* UV_COORD = _glCurrentTexCoord0();
    float* ST_COORD = _glCurrentTexCoord1();
    float* NORMAL = _glCurrentNormal();

    punned_t dest = { .flt = &vert->x };
    *(dest.flt++) = x;
    *(dest.flt++) = y;
    *(dest.flt++) = z;
    *(dest.flt++) = UV_COORD[0];
    *(dest.flt++) = UV_COORD[1];
    *(dest.flt++) = ST_COORD[0];
    *(dest.flt++) = ST_COORD[1];
    *(dest.flt++) = COLOR[0];
    *(dest.flt++) = COLOR[1];
    *(dest.flt++) = COLOR[2];
    *(dest.flt++) = COLOR[3];
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
    float* UV_COORD = _glCurrentTexCoord0();
    float* ST_COORD = _glCurrentTexCoord1();

    if(target == GL_TEXTURE0) {
        IM_ATTRIBS.enabled |= UV_ENABLED_FLAG;
        UV_COORD[0] = s;
        UV_COORD[1] = t;
    } else if(target == GL_TEXTURE1) {
        IM_ATTRIBS.enabled |= ST_ENABLED_FLAG;
        ST_COORD[0] = s;
        ST_COORD[1] = t;
    } else {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }
}

void APIENTRY glTexCoord1f(GLfloat u) {
    IM_ATTRIBS.enabled |= UV_ENABLED_FLAG;
    float* UV_COORD = _glCurrentTexCoord0();

    UV_COORD[0] = u;
    UV_COORD[1] = 0.0f;
}

void APIENTRY glTexCoord1fv(const GLfloat* v) {
    glTexCoord1f(v[0]);
}

void APIENTRY glTexCoord2f(GLfloat u, GLfloat v) {
    IM_ATTRIBS.enabled |= UV_ENABLED_FLAG;
    float* UV_COORD = _glCurrentTexCoord0();

    UV_COORD[0] = u;
    UV_COORD[1] = v;
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    glTexCoord2f(v[0], v[1]);
}

void APIENTRY glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    IM_ATTRIBS.enabled |= NORMAL_ENABLED_FLAG;
    float* NORMAL = _glCurrentNormal();

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
    IM_ATTRIBS.uv.ptr     = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, u);
    IM_ATTRIBS.st.ptr     = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, s);
    IM_ATTRIBS.colour.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, argb);
    IM_ATTRIBS.normal.ptr = IM_ATTRIBS.vertex.ptr + offsetof(IMVertex, nx);

    /* Redirect attrib state */
    AttribPointerList stashed_state = ATTRIB_LIST;
    ATTRIB_LIST = IM_ATTRIBS;

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, aligned_vector_header(&VERTICES)->size);

    ATTRIB_LIST = stashed_state;

    aligned_vector_clear(&VERTICES);
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
