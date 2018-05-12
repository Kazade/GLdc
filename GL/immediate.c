#include "../include/gl.h"
#include "private.h"

static GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector COLOURS;
static AlignedVector TEXCOORDS;

void initImmediateMode() {
    aligned_vector_init(&VERTICES, sizeof(GLfloat));
    aligned_vector_init(&COLOURS, sizeof(GLfloat));
    aligned_vector_init(&TEXCOORDS, sizeof(GLfloat));
}

GLubyte checkImmediateModeInactive(const char* func) {
    /* Returns 1 on error */
    if(IMMEDIATE_MODE_ACTIVE) {
        _glKosThrowError(GL_INVALID_OPERATION, func);
        _glKosPrintError();
        return 1;
    }

    return 0;
}

void APIENTRY glBegin(GLenum mode) {
    if(IMMEDIATE_MODE_ACTIVE) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    IMMEDIATE_MODE_ACTIVE = GL_TRUE;
    ACTIVE_POLYGON_MODE = mode;
}

void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    aligned_vector_push_back(&COLOURS, &r, 1);
    aligned_vector_push_back(&COLOURS, &g, 1);
    aligned_vector_push_back(&COLOURS, &b, 1);
    aligned_vector_push_back(&COLOURS, &a, 1);
}

void APIENTRY glColor4fv(const GLfloat* v) {
    glColor4f(v[0], v[1], v[2], v[3]);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    static float a = 1.0f;
    glColor4f(r, g, b, a);
}

void APIENTRY glColor3fv(const GLfloat* v) {
    glColor3f(v[0], v[1], v[2]);
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    aligned_vector_push_back(&VERTICES, &x, 1);
    aligned_vector_push_back(&VERTICES, &y, 1);
    aligned_vector_push_back(&VERTICES, &z, 1);
}

void APIENTRY glVertex3fv(const GLfloat* v) {
    glVertex3f(v[0], v[1], v[2]);
}

void APIENTRY glVertex4fv(const GLfloat* v) {
    glVertex4f(v[0], v[1], v[2], v[3]);
}

void APIENTRY glTexCoord2f(GLfloat u, GLfloat v) {
    aligned_vector_push_back(&TEXCOORDS, &u, 1);
    aligned_vector_push_back(&TEXCOORDS, &v, 1);
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    glTexCoord2f(v[0], v[1]);
}

void APIENTRY glEnd() {
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* FIXME: Push pointer state */

    glVertexPointer(3, GL_FLOAT, 0, VERTICES.data);
    glColorPointer(4, GL_FLOAT, 0, COLOURS.data);

    glClientActiveTextureARB(GL_TEXTURE0);
    glTexCoordPointer(2, GL_FLOAT, 0, TEXCOORDS.data);

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size / 3);

    aligned_vector_clear(&VERTICES);
    aligned_vector_clear(&COLOURS);
    aligned_vector_clear(&TEXCOORDS);

    /* FIXME: Pop pointers */
}
