/*
 * This implements immediate mode over the top of glDrawArrays
 * current problems:
 *
 * 1. Calling glNormal(); glVertex(); glVertex(); glVertex(); will break.
 * 2. Mixing with glXPointer stuff will break badly
 * 3. This is entirely untested.
 */

#include "../include/gl.h"
#include "../include/glext.h"
#include "profiler.h"

#include "private.h"

static GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector COLOURS;
static AlignedVector UV_COORDS;
static AlignedVector ST_COORDS;
static AlignedVector NORMALS;


static GLfloat NORMAL[3] = {0.0f, 0.0f, 1.0f};
static GLubyte COLOR[4] = {255, 255, 255, 255};
static GLfloat UV_COORD[2] = {0.0f, 0.0f};
static GLfloat ST_COORD[2] = {0.0f, 0.0f};


static AttribPointer VERTEX_ATTRIB;
static AttribPointer DIFFUSE_ATTRIB;
static AttribPointer UV_ATTRIB;
static AttribPointer ST_ATTRIB;
static AttribPointer NORMAL_ATTRIB;

void _glInitImmediateMode(GLuint initial_size) {
    aligned_vector_init(&VERTICES, sizeof(GLfloat));
    aligned_vector_init(&COLOURS, sizeof(GLubyte));
    aligned_vector_init(&UV_COORDS, sizeof(GLfloat));
    aligned_vector_init(&ST_COORDS, sizeof(GLfloat));
    aligned_vector_init(&NORMALS, sizeof(GLfloat));

    aligned_vector_reserve(&VERTICES, initial_size);
    aligned_vector_reserve(&COLOURS, initial_size);
    aligned_vector_reserve(&UV_COORDS, initial_size);
    aligned_vector_reserve(&ST_COORDS, initial_size);
    aligned_vector_reserve(&NORMALS, initial_size);

    VERTEX_ATTRIB.ptr = VERTICES.data;
    VERTEX_ATTRIB.size = 3;
    VERTEX_ATTRIB.type = GL_FLOAT;
    VERTEX_ATTRIB.stride = 0;

    DIFFUSE_ATTRIB.ptr = COLOURS.data;
    DIFFUSE_ATTRIB.size = 4;
    DIFFUSE_ATTRIB.type = GL_UNSIGNED_BYTE;
    DIFFUSE_ATTRIB.stride = 0;

    UV_ATTRIB.ptr = UV_COORDS.data;
    UV_ATTRIB.stride = 0;
    UV_ATTRIB.type = GL_FLOAT;
    UV_ATTRIB.size = 2;

    ST_ATTRIB.ptr = ST_COORDS.data;
    ST_ATTRIB.stride = 0;
    ST_ATTRIB.type = GL_FLOAT;
    ST_ATTRIB.size = 2;

    NORMAL_ATTRIB.ptr = NORMALS.data;
    NORMAL_ATTRIB.stride = 0;
    NORMAL_ATTRIB.type = GL_FLOAT;
    NORMAL_ATTRIB.size = 3;
}

GLubyte _glCheckImmediateModeInactive(const char* func) {
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
    COLOR[0] = (GLubyte)(r * 255);
    COLOR[1] = (GLubyte)(g * 255);
    COLOR[2] = (GLubyte)(b * 255);
    COLOR[3] = (GLubyte)(a * 255);
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    COLOR[0] = r;
    COLOR[1] = g;
    COLOR[2] = b;
    COLOR[3] = a;
}

void APIENTRY glColor4fv(const GLfloat* v) {
    COLOR[0] = (GLubyte)(v[0] * 255);
    COLOR[1] = (GLubyte)(v[1] * 255);
    COLOR[2] = (GLubyte)(v[2] * 255);
    COLOR[3] = (GLubyte)(v[3] * 255);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    COLOR[0] = (GLubyte)(r * 255);
    COLOR[1] = (GLubyte)(g * 255);
    COLOR[2] = (GLubyte)(b * 255);
    COLOR[3] = 255;
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    COLOR[0] = red;
    COLOR[1] = green;
    COLOR[2] = blue;
    COLOR[3] = 255;
}

void APIENTRY glColor3ubv(const GLubyte *v) {
    COLOR[0] = v[0];
    COLOR[1] = v[1];
    COLOR[2] = v[2];
    COLOR[3] = 255;
}

void APIENTRY glColor3fv(const GLfloat* v) {
    COLOR[0] = (GLubyte)(v[0] * 255);
    COLOR[1] = (GLubyte)(v[1] * 255);
    COLOR[2] = (GLubyte)(v[2] * 255);
    COLOR[3] = 255;
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    aligned_vector_reserve(&VERTICES, VERTICES.size + 3);
    aligned_vector_push_back(&VERTICES, &x, 1);
    aligned_vector_push_back(&VERTICES, &y, 1);
    aligned_vector_push_back(&VERTICES, &z, 1);


    /* Push back the stashed colour, normal and uv_coordinate */
    aligned_vector_push_back(&COLOURS, COLOR, 4);
    aligned_vector_push_back(&UV_COORDS, UV_COORD, 2);
    aligned_vector_push_back(&ST_COORDS, ST_COORD, 2);
    aligned_vector_push_back(&NORMALS, NORMAL, 3);
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
        _glKosPrintError();
        return;
    }
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
    profiler_push(__func__);

    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* Resizing could have invalidated these pointers */
    VERTEX_ATTRIB.ptr = VERTICES.data;
    DIFFUSE_ATTRIB.ptr = COLOURS.data;
    UV_ATTRIB.ptr = UV_COORDS.data;
    ST_ATTRIB.ptr = ST_COORDS.data;
    NORMAL_ATTRIB.ptr = NORMALS.data;

    GLuint* attrs = _glGetEnabledAttributes();

    AttribPointer* vattr = _glGetVertexAttribPointer();
    AttribPointer* dattr = _glGetDiffuseAttribPointer();
    AttribPointer* nattr = _glGetNormalAttribPointer();
    AttribPointer* uattr = _glGetUVAttribPointer();
    AttribPointer* sattr = _glGetSTAttribPointer();

    /* Stash existing values */
    AttribPointer vptr = *vattr;
    AttribPointer dptr = *dattr;
    AttribPointer nptr = *nattr;
    AttribPointer uvptr = *uattr;
    AttribPointer stptr = *sattr;

    GLuint prevAttrs = *attrs;

    /* Switch to our immediate mode arrays */
    *vattr = VERTEX_ATTRIB;
    *dattr = DIFFUSE_ATTRIB;
    *nattr = NORMAL_ATTRIB;
    *uattr = UV_ATTRIB;
    *sattr = ST_ATTRIB;

    *attrs = ~0;  // Enable everything

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size / 3);

    /* Restore everything */
    *vattr = vptr;
    *dattr = dptr;
    *nattr = nptr;
    *uattr = uvptr;
    *sattr = stptr;

    *attrs = prevAttrs;

    /* Clear arrays for next polys */
    aligned_vector_clear(&VERTICES);
    aligned_vector_clear(&COLOURS);
    aligned_vector_clear(&UV_COORDS);
    aligned_vector_clear(&ST_COORDS);
    aligned_vector_clear(&NORMALS);

    *vattr = vptr;
    *dattr = dptr;
    *nattr = nptr;
    *uattr = uvptr;
    *sattr = stptr;

    profiler_checkpoint("restore");
    profiler_pop();
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
