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

#include "profiler.h"
#include "private.h"

static GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector ST_COORDS;
static AlignedVector NORMALS;

static uint32_t NORMAL; /* Packed normal */
static GLubyte COLOR[4] = {255, 255, 255, 255};
static GLfloat UV_COORD[2] = {0.0f, 0.0f};
static GLfloat ST_COORD[2] = {0.0f, 0.0f};

static AttribPointer VERTEX_ATTRIB;
static AttribPointer DIFFUSE_ATTRIB;
static AttribPointer UV_ATTRIB;
static AttribPointer ST_ATTRIB;
static AttribPointer NORMAL_ATTRIB;

/* Set of flags that have been enabled by glNormal etc. Cleared to VERTEX_ENABLED_FLAG
   in glBegin */
static GLuint ENABLED_VERTEX_ATTRIBUTES_DRAFT = VERTEX_ENABLED_FLAG;

/* Set from ENABLED_VERTEX_ATTRIBUTES when glVertex is called so that
 * new attribute types are ignored if they appear after the first vertex */
static GLuint ENABLED_VERTEX_ATTRIBUTES = 0;

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

    /* Only vertices enabled at the moment */
    ENABLED_VERTEX_ATTRIBUTES = ENABLED_VERTEX_ATTRIBUTES_DRAFT = VERTEX_ENABLED_FLAG;
}

void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = (GLubyte)(r * 255);
    COLOR[1] = (GLubyte)(g * 255);
    COLOR[2] = (GLubyte)(b * 255);
    COLOR[3] = (GLubyte)(a * 255);
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = r;
    COLOR[1] = g;
    COLOR[2] = b;
    COLOR[3] = a;
}

void APIENTRY glColor4fv(const GLfloat* v) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = (GLubyte)(v[0] * 255);
    COLOR[1] = (GLubyte)(v[1] * 255);
    COLOR[2] = (GLubyte)(v[2] * 255);
    COLOR[3] = (GLubyte)(v[3] * 255);
}

void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = (GLubyte)(r * 255);
    COLOR[1] = (GLubyte)(g * 255);
    COLOR[2] = (GLubyte)(b * 255);
    COLOR[3] = 255;
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = red;
    COLOR[1] = green;
    COLOR[2] = blue;
    COLOR[3] = 255;
}

void APIENTRY glColor3ubv(const GLubyte *v) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = v[0];
    COLOR[1] = v[1];
    COLOR[2] = v[2];
    COLOR[3] = 255;
}

void APIENTRY glColor3fv(const GLfloat* v) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= DIFFUSE_ENABLED_FLAG;

    COLOR[0] = (GLubyte)(v[0] * 255);
    COLOR[1] = (GLubyte)(v[1] * 255);
    COLOR[2] = (GLubyte)(v[2] * 255);
    COLOR[3] = 255;
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    ENABLED_VERTEX_ATTRIBUTES = ENABLED_VERTEX_ATTRIBUTES_DRAFT;

    GLVertexKOS* vert = aligned_vector_extend(&VERTICES, 1);

    vert->x = x;
    vert->y = y;
    vert->z = z;

    if(ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) {
        vert->u = UV_COORD[0];
        vert->v = UV_COORD[1];
    }

    if(ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) {
        vert->bgra[R8IDX] = COLOR[0];
        vert->bgra[G8IDX] = COLOR[1];
        vert->bgra[B8IDX] = COLOR[2];
        vert->bgra[A8IDX] = COLOR[3];
    }

    if(ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) {
        GLuint* n = aligned_vector_extend(&NORMALS, 1);
        *n = NORMAL;
    }

    if(ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) {
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
        ENABLED_VERTEX_ATTRIBUTES_DRAFT |= UV_ENABLED_FLAG;
        UV_COORD[0] = s;
        UV_COORD[1] = t;
    } else if(target == GL_TEXTURE1) {
        ENABLED_VERTEX_ATTRIBUTES_DRAFT |= ST_ENABLED_FLAG;
        ST_COORD[0] = s;
        ST_COORD[1] = t;
    } else {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return;
    }
}

void APIENTRY glTexCoord2f(GLfloat u, GLfloat v) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= UV_ENABLED_FLAG;
    UV_COORD[0] = u;
    UV_COORD[1] = v;
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    glTexCoord2f(v[0], v[1]);
}

void APIENTRY glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= NORMAL_ENABLED_FLAG;
    NORMAL = pack_vertex_attribute_vec3_1i(x, y, z);
}

void APIENTRY glNormal3fv(const GLfloat* v) {
    ENABLED_VERTEX_ATTRIBUTES_DRAFT |= NORMAL_ENABLED_FLAG;
    glNormal3f(v[0], v[1], v[2]);
}

void APIENTRY glEnd() {
    profiler_push(__func__);

    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    /* Resizing could have invalidated these pointers */
    VERTEX_ATTRIB.ptr = VERTICES.data + sizeof(uint32_t);
    UV_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 3);
    DIFFUSE_ATTRIB.ptr = VERTEX_ATTRIB.ptr + (sizeof(GLfloat) * 5);

    NORMAL_ATTRIB.ptr = NORMALS.data;
    ST_ATTRIB.ptr = ST_COORDS.data;

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

    *attrs = ENABLED_VERTEX_ATTRIBUTES;

#ifndef NDEBUG
    _glRecalcFastPath();
#else
    // Immediate mode should always activate the fast path
    GLboolean fastPathEnabled = _glRecalcFastPath();
    assert(fastPathEnabled);
#endif

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size);

    /* Restore everything */
    *vattr = vptr;
    *dattr = dptr;
    *nattr = nptr;
    *uattr = uvptr;
    *sattr = stptr;

    *attrs = prevAttrs;

    /* Clear arrays for next polys */
    aligned_vector_clear(&VERTICES);
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
