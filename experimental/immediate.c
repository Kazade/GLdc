#include "../include/gl.h"
#include "private.h"

static GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector COLOURS;

void initImmediateMode() {
    aligned_vector_init(&VERTICES, sizeof(GLfloat));
    aligned_vector_init(&COLOURS, sizeof(GLfloat));
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

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    static float a = 1.0f;
    glColor4f(r, g, b, a);
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    aligned_vector_push_back(&VERTICES, &x, 1);
    aligned_vector_push_back(&VERTICES, &y, 1);
    aligned_vector_push_back(&VERTICES, &z, 1);
}

void APIENTRY glEnd() {
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* FIXME: Push pointers */

    glVertexPointer(3, GL_FLOAT, 0, VERTICES.data);
    glColorPointer(4, GL_FLOAT, 0, COLOURS.data);

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size / 3);

    aligned_vector_clear(&VERTICES);
    aligned_vector_clear(&COLOURS);

    /* FIXME: Pop pointers */
}
