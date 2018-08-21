/*
 * This implements immediate mode over the top of glDrawArrays
 * current problems:
 *
 * 1. Calling glNormal(); glVertex(); glVertex(); glVertex(); will break.
 * 2. Mixing with glXPointer stuff will break badly
 * 3. This is entirely untested.
 */

#include "../include/gl.h"
#include "private.h"

static GLboolean IMMEDIATE_MODE_ACTIVE = GL_FALSE;
static GLenum ACTIVE_POLYGON_MODE = GL_TRIANGLES;

static AlignedVector VERTICES;
static AlignedVector COLOURS;
static AlignedVector UV_COORDS;
static AlignedVector ST_COORDS;
static AlignedVector NORMALS;


static GLfloat NORMAL[3] = {0.0f, 0.0f, 1.0f};
static GLfloat COLOR[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static GLfloat UV_COORD[2] = {0.0f, 0.0f};
static GLfloat ST_COORD[2] = {0.0f, 0.0f};


void initImmediateMode() {
    aligned_vector_init(&VERTICES, sizeof(GLfloat));
    aligned_vector_init(&COLOURS, sizeof(GLfloat));
    aligned_vector_init(&UV_COORDS, sizeof(GLfloat));
    aligned_vector_init(&ST_COORDS, sizeof(GLfloat));
    aligned_vector_init(&NORMALS, sizeof(GLfloat));
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
    COLOR[0] = r;
    COLOR[1] = g;
    COLOR[2] = b;
    COLOR[3] = a;
}

void APIENTRY glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    glColor4f(
        ((GLfloat) r) / 255.0f,
        ((GLfloat) g) / 255.0f,
        ((GLfloat) b) / 255.0f,
        ((GLfloat) a) / 255.0f
    );
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
    IMMEDIATE_MODE_ACTIVE = GL_FALSE;

    GLboolean vertexArrayEnabled, colorArrayEnabled, normalArrayEnabled;
    GLboolean texArray0Enabled, texArray1Enabled;

    glGetBooleanv(GL_VERTEX_ARRAY, &vertexArrayEnabled);
    glGetBooleanv(GL_COLOR_ARRAY, &colorArrayEnabled);
    glGetBooleanv(GL_NORMAL_ARRAY, &normalArrayEnabled);

    /* FIXME: Push pointer state */

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, VERTICES.data);
    glColorPointer(4, GL_FLOAT, 0, COLOURS.data);
    glNormalPointer(GL_FLOAT, 0, NORMALS.data);

    GLint activeTexture;
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &activeTexture);

    glClientActiveTextureARB(GL_TEXTURE0);
    glGetBooleanv(GL_TEXTURE_COORD_ARRAY, &texArray0Enabled);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, UV_COORDS.data);

    glClientActiveTextureARB(GL_TEXTURE1);
    glGetBooleanv(GL_TEXTURE_COORD_ARRAY, &texArray1Enabled);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, ST_COORDS.data);

    glDrawArrays(ACTIVE_POLYGON_MODE, 0, VERTICES.size / 3);

    aligned_vector_clear(&VERTICES);
    aligned_vector_clear(&COLOURS);
    aligned_vector_clear(&UV_COORDS);
    aligned_vector_clear(&ST_COORDS);
    aligned_vector_clear(&NORMALS);

    if(!vertexArrayEnabled) {
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    if(!colorArrayEnabled) {
        glDisableClientState(GL_COLOR_ARRAY);
    }

    if(!normalArrayEnabled) {
        glDisableClientState(GL_NORMAL_ARRAY);
    }

    if(!texArray0Enabled) {
        glClientActiveTextureARB(GL_TEXTURE0);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    if(!texArray1Enabled) {
        glClientActiveTextureARB(GL_TEXTURE1);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glClientActiveTextureARB((GLuint) activeTexture);


    /* FIXME: Pop pointers */
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
