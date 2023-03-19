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

GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static GLfloat NORMAL[3] = {0.0f, 0.0f, 1.0f};
static GLubyte COLOR[4] = {255, 255, 255, 255}; /* ARGB order for speed */
static GLfloat UV_COORD[2] = {0.0f, 0.0f};
static GLfloat ST_COORD[2] = {0.0f, 0.0f};

static AlignedVector VERTICES;

typedef struct {
    uint32_t padding;
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLubyte bgra[4];
    GLubyte obgra[4];
    GLfloat nx;
    GLfloat ny;
    GLfloat nz;
    GLfloat s;
    GLfloat t;
    GLuint padding2[3];
} IMVertex;


void _glInitImmediateMode(GLuint initial_size) {
    aligned_vector_init(&VERTICES, sizeof(IMVertex));
    aligned_vector_reserve(&VERTICES, initial_size);
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
    COLOR[A8IDX] = (GLubyte)(a * 255.0f);
    COLOR[R8IDX] = (GLubyte)(r * 255.0f);
    COLOR[G8IDX] = (GLubyte)(g * 255.0f);
    COLOR[B8IDX] = (GLubyte)(b * 255.0f);
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    COLOR[A8IDX] = a;
    COLOR[R8IDX] = r;
    COLOR[G8IDX] = g;
    COLOR[B8IDX] = b;
}

void APIENTRY glColor4ubv(const GLubyte *v) {
    COLOR[A8IDX] = v[3];
    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
}

void APIENTRY glColor4fv(const GLfloat* v) {
    COLOR[B8IDX] = (GLubyte)(v[2] * 255);
    COLOR[G8IDX] = (GLubyte)(v[1] * 255);
    COLOR[R8IDX] = (GLubyte)(v[0] * 255);
    COLOR[A8IDX] = (GLubyte)(v[3] * 255);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    COLOR[B8IDX] = (GLubyte)(b * 255.0f);
    COLOR[G8IDX] = (GLubyte)(g * 255.0f);
    COLOR[R8IDX] = (GLubyte)(r * 255.0f);
    COLOR[A8IDX] = 255;
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = red;
    COLOR[G8IDX] = green;
    COLOR[B8IDX] = blue;
}

void APIENTRY glColor3ubv(const GLubyte *v) {
    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = v[0];
    COLOR[G8IDX] = v[1];
    COLOR[B8IDX] = v[2];
}

void APIENTRY glColor3fv(const GLfloat* v) {
    COLOR[A8IDX] = 255;
    COLOR[R8IDX] = (GLubyte)(v[0] * 255);
    COLOR[G8IDX] = (GLubyte)(v[1] * 255);
    COLOR[B8IDX] = (GLubyte)(v[2] * 255);
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    IMVertex* vert = aligned_vector_extend(&VERTICES, 1);

    vert->x = x;
    vert->y = y;
    vert->z = z;
    vert->u = UV_COORD[0];
    vert->v = UV_COORD[1];
    vert->s = ST_COORD[0];
    vert->t = ST_COORD[1];

    *((uint32_t*) vert->bgra) = *((uint32_t*) COLOR);

    vert->nx = NORMAL[0];
    vert->ny = NORMAL[1];
    vert->nz = NORMAL[2];
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
        UV_COORD[0] = s;
        UV_COORD[1] = t;
    } else if(target == GL_TEXTURE1) {
        ST_COORD[0] = s;
        ST_COORD[1] = t;
    } else {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }
}

void APIENTRY glTexCoord1f(GLfloat u) {
    UV_COORD[0] = u;
    UV_COORD[1] = 0.0f;
}

void APIENTRY glTexCoord1fv(const GLfloat* v) {
    glTexCoord1f(v[0]);
}

void APIENTRY glTexCoord2f(GLfloat u, GLfloat v) {
    UV_COORD[0] = u;
    UV_COORD[1] = v;
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    glTexCoord2f(v[0], v[1]);
}

void APIENTRY glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    NORMAL[0] = x;
    NORMAL[1] = y;
    NORMAL[2] = z;
}

void APIENTRY glNormal3fv(const GLfloat* v) {
    glNormal3f(v[0], v[1], v[2]);
}

void APIENTRY glEnd() {
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    glDrawPVRArrays64KOS(ACTIVE_POLYGON_MODE, 0, VERTICES.size, VERTICES.data);

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
