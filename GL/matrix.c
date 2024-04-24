#include <string.h>
#include <math.h>
#include <stdio.h>

#include "private.h"

#include "../containers/stack.h"

#define DEG2RAD (0.01745329251994329576923690768489f)


/* Depth range */
GLfloat DEPTH_RANGE_MULTIPLIER_L = (1 - 0) / 2;
GLfloat DEPTH_RANGE_MULTIPLIER_H = (0 + 1) / 2;

static Stack __attribute__((aligned(32))) MATRIX_STACKS[4]; // modelview, projection, texture
static Matrix4x4 __attribute__((aligned(32))) NORMAL_MATRIX;

Viewport VIEWPORT = {
    0, 0, 640, 480, 320.0f, 240.0f, 320.0f, 240.0f
};

static GLenum MATRIX_MODE = GL_MODELVIEW;
static GLubyte MATRIX_IDX = 0;

static const Matrix4x4 __attribute__((aligned(32))) IDENTITY = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

GLfloat NEAR_PLANE_DISTANCE = 0.0f;

Matrix4x4* _glGetProjectionMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[1]);
}

Matrix4x4* _glGetModelViewMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[0]);
}

void _glInitMatrices() {
    init_stack(&MATRIX_STACKS[0], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[1], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[2], sizeof(Matrix4x4), 32);

    stack_push(&MATRIX_STACKS[0], IDENTITY);
    stack_push(&MATRIX_STACKS[1], IDENTITY);
    stack_push(&MATRIX_STACKS[2], IDENTITY);

    MEMCPY4(NORMAL_MATRIX, IDENTITY, sizeof(Matrix4x4));

    const VideoMode* vid_mode = GetVideoMode();

    glDepthRange(0.0f, 1.0f);
    glViewport(0, 0, vid_mode->width, vid_mode->height);
}

#define swap(a, b) { \
    GLfloat x = (a); \
    a = b; \
    b = x; \
}

static void inverse(GLfloat* m) {
    GLfloat f4 = m[4];
    GLfloat f8 = m[8];
    GLfloat f1 = m[1];
    GLfloat f9 = m[9];
    GLfloat f2 = m[2];
    GLfloat f6 = m[6];
    GLfloat f12 = m[12];
    GLfloat f13 = m[13];
    GLfloat f14 = m[14];

    m[1]  =  f4;
    m[2]  =  f8;
    m[4]  =  f1;
    m[6]  =  f9;
    m[8]  =  f2;
    m[9]  =  f6;
    m[12] = -(f12 * m[0]  +  f13 * m[4]  +  f14 * m[8]);
    m[13] = -(f12 * m[1]  +  f13 * m[5]  +  f14 * m[9]);
    m[14] = -(f12 * m[2]  +  f13 * m[6]  +  f14 * m[10]);
}

static void transpose(GLfloat* m) {
    swap(m[1], m[4]);
    swap(m[2], m[8]);
    swap(m[3], m[12]);
    swap(m[6], m[9]);
    swap(m[7], m[3]);
    swap(m[11], m[14]);
}

static void recalculateNormalMatrix() {
    MEMCPY4(NORMAL_MATRIX, stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)), sizeof(Matrix4x4));
    inverse((GLfloat*) NORMAL_MATRIX);
    transpose((GLfloat*) NORMAL_MATRIX);
}

void APIENTRY glMatrixMode(GLenum mode) {
    MATRIX_MODE = mode;
    MATRIX_IDX = mode & 0xF;
}

void APIENTRY glPushMatrix() {
    void* top = stack_top(MATRIX_STACKS + MATRIX_IDX);
    assert(top);
    void* ret = stack_push(MATRIX_STACKS + MATRIX_IDX, top);
    (void) ret;
    assert(ret);
}

void APIENTRY glPopMatrix() {
    stack_pop(MATRIX_STACKS + MATRIX_IDX);
    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

void APIENTRY glLoadIdentity() {
    stack_replace(MATRIX_STACKS + MATRIX_IDX, IDENTITY);
}

void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    const Matrix4x4 trn __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, z, 1.0f
    };
    void* top = stack_top(MATRIX_STACKS + MATRIX_IDX);
    assert(top);

    UploadMatrix4x4(top);
    MultiplyMatrix4x4(&trn);

    top = stack_top(MATRIX_STACKS + MATRIX_IDX);
    assert(top);

    DownloadMatrix4x4(top);

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}


void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) {
    const Matrix4x4 scale __attribute__((aligned(32))) = {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4(&scale);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat  y, GLfloat z) {
    Matrix4x4 rotate __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    float r = DEG2RAD * angle;
    float c = cosf(r);
    float s = sinf(r);

    VEC3_NORMALIZE(x, y, z);

    float invc = 1.0f - c;
    float xs = x * s;
    float zs = z * s;
    float ys = y * s;
    float xz = x * z;
    float xy = y * x;
    float yz = y * z;

    rotate[M0] = (x * x) * invc + c;
    rotate[M1] = xy * invc + zs;
    rotate[M2] = xz * invc - ys;

    rotate[M4] = xy * invc - zs;
    rotate[M5] = (y * y) * invc + c;
    rotate[M6] = yz * invc + xs;

    rotate[M8] = xz * invc + ys;
    rotate[M9] = yz * invc - xs;
    rotate[M10] = (z * z) * invc + c;

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4((const Matrix4x4*) &rotate);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Load an arbitrary matrix */
void APIENTRY glLoadMatrixf(const GLfloat *m) {
    static Matrix4x4 __attribute__((aligned(32))) TEMP;

    memcpy(TEMP, m, sizeof(float) * 16);
    stack_replace(MATRIX_STACKS + MATRIX_IDX, TEMP);

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Ortho */
void APIENTRY glOrtho(GLfloat left, GLfloat right,
             GLfloat bottom, GLfloat top,
             GLfloat znear, GLfloat zfar) {

    /* Ortho Matrix */
    Matrix4x4 OrthoMatrix __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    OrthoMatrix[M0] = 2.0f / (right - left);
    OrthoMatrix[M5] = 2.0f / (top - bottom);
    OrthoMatrix[M10] = -2.0f / (zfar - znear);
    OrthoMatrix[M12] = -(right + left) / (right - left);
    OrthoMatrix[M13] = -(top + bottom) / (top - bottom);
    OrthoMatrix[M14] = -(zfar + znear) / (zfar - znear);

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4((const Matrix4x4*) &OrthoMatrix);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
}


/* Set the GL frustum */
void APIENTRY glFrustum(GLfloat left, GLfloat right,
               GLfloat bottom, GLfloat top,
               GLfloat znear, GLfloat zfar) {

    /* Frustum Matrix */
    Matrix4x4 FrustumMatrix __attribute__((aligned(32)));

    MEMSET(FrustumMatrix, 0, sizeof(float) * 16);

    const float near2 = 2.0f * znear;
    const float A = (right + left) / (right - left);
    const float B = (top + bottom) / (top - bottom);
    const float C = -((zfar + znear) / (zfar - znear));
    const float D = -((2.0f * zfar * znear) / (zfar - znear));

    FrustumMatrix[M0] = near2 / (right - left);
    FrustumMatrix[M5] = near2 / (top - bottom);

    FrustumMatrix[M8] = A;
    FrustumMatrix[M9] = B;
    FrustumMatrix[M10] = C;
    FrustumMatrix[M11] = -1.0f;
    FrustumMatrix[M14] = D;

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4((const Matrix4x4*) &FrustumMatrix);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
}


/* Multiply the current matrix by an arbitrary matrix */
void glMultMatrixf(const GLfloat *m) {
    Matrix4x4 TEMP __attribute__((aligned(32)));
    MEMCPY4(TEMP, m, sizeof(Matrix4x4));

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4(&TEMP);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Load an arbitrary transposed matrix */
void glLoadTransposeMatrixf(const GLfloat *m) {
    /* We store matrices transpose anyway, so m will be
     * transpose compared to all other matrices */

    static Matrix4x4 TEMP __attribute__((aligned(32)));

    TEMP[M0] = m[0];
    TEMP[M1] = m[4];
    TEMP[M2] = m[8];
    TEMP[M3] = m[12];

    TEMP[M4] = m[1];
    TEMP[M5] = m[5];
    TEMP[M6] = m[9];
    TEMP[M7] = m[13];

    TEMP[M8] = m[3];
    TEMP[M9] = m[6];
    TEMP[M10] = m[10];
    TEMP[M11] = m[14];

    TEMP[M12] = m[4];
    TEMP[M13] = m[7];
    TEMP[M14] = m[11];
    TEMP[M15] = m[15];

    stack_replace(MATRIX_STACKS + MATRIX_IDX, TEMP);

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Multiply the current matrix by an arbitrary transposed matrix */
void glMultTransposeMatrixf(const GLfloat *m) {
    static Matrix4x4 TEMP __attribute__((aligned(32)));

    TEMP[M0] = m[0];
    TEMP[M1] = m[4];
    TEMP[M2] = m[8];
    TEMP[M3] = m[12];

    TEMP[M4] = m[1];
    TEMP[M5] = m[5];
    TEMP[M6] = m[9];
    TEMP[M7] = m[13];

    TEMP[M8] = m[3];
    TEMP[M9] = m[6];
    TEMP[M10] = m[10];
    TEMP[M11] = m[14];

    TEMP[M12] = m[4];
    TEMP[M13] = m[7];
    TEMP[M14] = m[11];
    TEMP[M15] = m[15];

    UploadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));
    MultiplyMatrix4x4((const Matrix4x4*) &TEMP);
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Set the GL viewport */
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    VIEWPORT.x = x;
    VIEWPORT.y = y;
    VIEWPORT.width = width;
    VIEWPORT.height = height;
    VIEWPORT.hwidth = ((GLfloat) VIEWPORT.width) * 0.5f;
    VIEWPORT.hheight = ((GLfloat) VIEWPORT.height) * 0.5f;
    VIEWPORT.x_plus_hwidth = VIEWPORT.x + VIEWPORT.hwidth;
    VIEWPORT.y_plus_hheight = VIEWPORT.y + VIEWPORT.hheight;
}

/* Set the depth range */
void APIENTRY glDepthRangef(GLclampf n, GLclampf f) {
    if(n < 0.0f) n = 0.0f;
    else if(n > 1.0f) n = 1.0f;

    if(f < 0.0f) f = 0.0f;
    else if(f > 1.0f) f = 1.0f;

    DEPTH_RANGE_MULTIPLIER_L = (f - n) / 2.0f;
    DEPTH_RANGE_MULTIPLIER_H = (n + f) / 2.0f;
}

void APIENTRY glDepthRange(GLclampf n, GLclampf f){
    glDepthRangef(n,f);
}

/* Vector Cross Product - Used by gluLookAt */
static inline void vec3f_cross(const GLfloat* v1, const GLfloat* v2, GLfloat* result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

GL_FORCE_INLINE void vec3f_normalize_sh4(float *v){
    float length, ilength;

    ilength = MATH_fsrra(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    length = MATH_Fast_Invert(ilength);
    if (length)
    {
        v[0] *= ilength;
        v[1] *= ilength;
        v[2] *= ilength;
    }
}

void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx,
               GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy,
               GLfloat upz) {
    GLfloat m [16] __attribute__((aligned(32)));
    GLfloat f [3];
    GLfloat u [3];
    GLfloat s [3];

    f[0] = centerx - eyex;
    f[1] = centery - eyey;
    f[2] = centerz - eyez;

    u[0] = upx;
    u[1] = upy;
    u[2] = upz;

    vec3f_normalize_sh4(f);
    vec3f_cross(f, u, s);
    vec3f_normalize_sh4(s);
    vec3f_cross(s, f, u);

    m[0] =  s[0]; m[4] =  s[1]; m[8] =   s[2]; m[12] = 0.0f;
    m[1] =  u[0]; m[5] =  u[1]; m[9] =   u[2]; m[13] = 0.0f;
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = 0.0f;
    m[3] =  0.0f; m[7] =  0.0f; m[11] =  0.0f; m[15] = 1.0f;

    static Matrix4x4 trn __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    trn[M12] = -eyex;
    trn[M13] = -eyey;
    trn[M14] = -eyez;

    // Does not modify internal Modelview matrix
    UploadMatrix4x4((const Matrix4x4*) &m);
    MultiplyMatrix4x4((const Matrix4x4*) &trn);
    MultiplyMatrix4x4(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
    DownloadMatrix4x4(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadTexture() {
    UploadMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_TEXTURE & 0xF)));
}

void _glMatrixLoadModelView() {
    UploadMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadProjection() {
    UploadMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_PROJECTION & 0xF)));
}

void _glMatrixLoadModelViewProjection() {
    UploadMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_PROJECTION & 0xF)));
    MultiplyMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadNormal() {
    UploadMatrix4x4((const Matrix4x4*) &NORMAL_MATRIX);
}
