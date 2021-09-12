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

extern inline GLboolean _glRecalcFastPath();

GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector ST_COORDS;
static AlignedVector NORMALS;

static uint32_t NORMAL; /* Packed normal */
static GLubyte COLOR[4] = {255, 255, 255, 255}; /* ARGB order for speed */
static GLfloat UV_COORD[2] = {0.0f, 0.0f};
static GLfloat ST_COORD[2] = {0.0f, 0.0f};

static AttribPointer VERTEX_ATTRIB;
static AttribPointer DIFFUSE_ATTRIB;
static AttribPointer UV_ATTRIB;
static AttribPointer ST_ATTRIB;
static AttribPointer NORMAL_ATTRIB;

extern AttribPointer VERTEX_POINTER;
extern AttribPointer UV_POINTER;
extern AttribPointer ST_POINTER;
extern AttribPointer NORMAL_POINTER;
extern AttribPointer DIFFUSE_POINTER;

/* We store the list of attributes that have been "enabled" by a call to
  glColor, glNormal, glTexCoord etc. otherwise we already have defaults that
  can be applied faster */
static GLuint IM_ENABLED_VERTEX_ATTRIBUTES = 0;

static inline uint32_t pack_vertex_attribute_vec3_1i(float x, float y, float z) {
    const float w = 0.0f;

    const uint32_t xs = x < 0;
    const uint32_t ys = y < 0;
    const uint32_t zs = z < 0;
    const uint32_t ws = w < 0;

    uint32_t vi =
        ws << 31 | ((uint32_t)(w + (ws << 1)) & 1) << 30 |
        zs << 29 | ((uint32_t)(z * 511 + (zs << 9)) & 511) << 20 |
        ys << 19 | ((uint32_t)(y * 511 + (ys << 9)) & 511) << 10 |
        xs << 9  | ((uint32_t)(x * 511 + (xs << 9)) & 511);

    return vi;
}

void _glInitImmediateMode(GLuint initial_size) {
    aligned_vector_init(&VERTICES, sizeof(GLVertexKOS));
    aligned_vector_init(&ST_COORDS, sizeof(GLfloat));
    aligned_vector_init(&NORMALS, sizeof(GLuint));

    aligned_vector_reserve(&VERTICES, initial_size);
    aligned_vector_reserve(&ST_COORDS, initial_size * 2);
    aligned_vector_reserve(&NORMALS, initial_size);

    VERTEX_ATTRIB.ptr = VERTICES.data + sizeof(uint32_t);
    VERTEX_ATTRIB.size = 3;
    VERTEX_ATTRIB.type = GL_FLOAT;
    VERTEX_ATTRIB.stride = 32;

    UV_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 3);
    UV_ATTRIB.stride = 32;
    UV_ATTRIB.type = GL_FLOAT;
    UV_ATTRIB.size = 2;

    DIFFUSE_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 5);
    DIFFUSE_ATTRIB.size = GL_BGRA;  /* Flipped color order */
    DIFFUSE_ATTRIB.type = GL_UNSIGNED_BYTE;
    DIFFUSE_ATTRIB.stride = 32;

    NORMAL_ATTRIB.ptr = NORMALS.data;
    NORMAL_ATTRIB.stride = 0;
    NORMAL_ATTRIB.type = GL_UNSIGNED_INT_2_10_10_10_REV;
    NORMAL_ATTRIB.size = 1;

    ST_ATTRIB.ptr = ST_COORDS.data;
    ST_ATTRIB.stride = 0;
    ST_ATTRIB.type = GL_FLOAT;
    ST_ATTRIB.size = 2;

    NORMAL = pack_vertex_attribute_vec3_1i(0.0f, 0.0f, 1.0f);
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

void APIENTRY glColor4fv(const GLfloat* v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[B8IDX] = (GLubyte)(v[2] * 255);
    COLOR[G8IDX] = (GLubyte)(v[1] * 255);
    COLOR[R8IDX] = (GLubyte)(v[0] * 255);
    COLOR[A8IDX] = (GLubyte)(v[3] * 255);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;

    COLOR[B8IDX] = (GLubyte)(b * 255);
    COLOR[G8IDX] = (GLubyte)(g * 255);
    COLOR[R8IDX] = (GLubyte)(r * 255);
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

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= VERTEX_ENABLED_FLAG;

    GLVertexKOS* vert = aligned_vector_extend(&VERTICES, 1);

    vert->x = x;
    vert->y = y;
    vert->z = z;
    vert->u = UV_COORD[0];
    vert->v = UV_COORD[1];
    *((uint32_t*) vert->bgra) = *((uint32_t*) COLOR);

    if(IM_ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) {
        GLuint* n = aligned_vector_extend(&NORMALS, 1);
        *n = NORMAL;
    }

    if(IM_ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) {
        GLfloat* st = aligned_vector_extend(&ST_COORDS, 2);
        st[0] = ST_COORD[0];
        st[1] = ST_COORD[1];
    }
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
        _glKosPrintError();
        return;
    }
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
    NORMAL = pack_vertex_attribute_vec3_1i(x, y, z);
}

void APIENTRY glNormal3fv(const GLfloat* v) {
    IM_ENABLED_VERTEX_ATTRIBUTES |= NORMAL_ENABLED_FLAG;
    glNormal3f(v[0], v[1], v[2]);
}

void APIENTRY glEnd() {
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* Resizing could have invalidated these pointers */
    VERTEX_ATTRIB.ptr = VERTICES.data + sizeof(uint32_t);
    UV_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 3);
    DIFFUSE_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 5);

    NORMAL_ATTRIB.ptr = NORMALS.data;
    ST_ATTRIB.ptr = ST_COORDS.data;

    GLuint* attrs = &ENABLED_VERTEX_ATTRIBUTES;

    /* Stash existing values */
    AttribPointer vptr = VERTEX_POINTER;
    AttribPointer dptr = DIFFUSE_POINTER;
    AttribPointer nptr = NORMAL_POINTER;
    AttribPointer uvptr = UV_POINTER;
    AttribPointer stptr = ST_POINTER;

    GLuint prevAttrs = *attrs;

    /* Switch to our immediate mode arrays */
    VERTEX_POINTER = VERTEX_ATTRIB;
    DIFFUSE_POINTER = DIFFUSE_ATTRIB;
    NORMAL_POINTER = NORMAL_ATTRIB;
    UV_POINTER = UV_ATTRIB;
    ST_POINTER = ST_ATTRIB;

    *attrs = IM_ENABLED_VERTEX_ATTRIBUTES;

#ifndef NDEBUG
    /* If we're not debugging, set to true - we assume we haven't broken it! */
    FAST_PATH_ENABLED = GL_TRUE;
#else
    // Immediate mode should always activate the fast path
    GLboolean fastPathEnabled = _glRecalcFastPath();
    assert(fastPathEnabled);
#endif

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size);

    /* Restore everything */
    VERTEX_POINTER = vptr;
    DIFFUSE_POINTER = dptr;
    NORMAL_POINTER = nptr;
    UV_POINTER = uvptr;
    ST_POINTER = stptr;

    *attrs = prevAttrs;

    /* Clear arrays for next polys */
    aligned_vector_clear(&VERTICES);
    aligned_vector_clear(&ST_COORDS);
    aligned_vector_clear(&NORMALS);
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
