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
static Matrix4x4 __attribute__((aligned(32))) VIEWPORT_MATRIX;
static Matrix4x4 __attribute__((aligned(32))) PROJECTION_MATRIX;

static GLenum MATRIX_MODE = GL_MODELVIEW;
static Stack* MATRIX_CUR;
static GLboolean NORMAL_DIRTY, PROJECTION_DIRTY;

static const Matrix4x4 __attribute__((aligned(32))) IDENTITY = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

GLfloat NEAR_PLANE_DISTANCE = 0.0f;

Matrix4x4* _glGetModelViewMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[0]);
}

Matrix4x4* _glGetProjectionMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[1]);
}

Matrix4x4* _glGetTextureMatrix() {
    return (Matrix4x4*) stack_top(MATRIX_STACKS + (GL_TEXTURE & 0xF));
}

Matrix4x4* _glGetColorMatrix() {
    return (Matrix4x4*) stack_top(MATRIX_STACKS + (GL_COLOR & 0xF));
}

GLenum _glGetMatrixMode() {
    return MATRIX_MODE;
}

GLboolean _glIsIdentity(const Matrix4x4* m) {
    return memcmp(m, IDENTITY, sizeof(Matrix4x4)) == 0;
}

void _glInitMatrices() {
    init_stack(&MATRIX_STACKS[0], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[1], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[2], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[3], sizeof(Matrix4x4), 32);

    stack_push(&MATRIX_STACKS[0], IDENTITY);
    stack_push(&MATRIX_STACKS[1], IDENTITY);
    stack_push(&MATRIX_STACKS[2], IDENTITY);
    stack_push(&MATRIX_STACKS[3], IDENTITY);

    MEMCPY4(NORMAL_MATRIX, IDENTITY, sizeof(Matrix4x4));

    MATRIX_CUR = MATRIX_STACKS + (GL_MODELVIEW & 0xF);

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

/* When projection matrix changes, need to pre-multiply with viewport transform matrix */
static void UpdateProjectionMatrix() {
    PROJECTION_DIRTY = false;
    UploadMatrix4x4(&VIEWPORT_MATRIX);
    MultiplyMatrix4x4(stack_top(MATRIX_STACKS + (GL_PROJECTION & 0xF)));
    DownloadMatrix4x4(&PROJECTION_MATRIX);
}

/* When modelview matrix changes, need to re-compute normal matrix */
static void UpdateNormalMatrix() {
    NORMAL_DIRTY = false;
    MEMCPY4(NORMAL_MATRIX, stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)), sizeof(Matrix4x4));
    inverse((GLfloat*) NORMAL_MATRIX);
    transpose((GLfloat*) NORMAL_MATRIX);
}

static void OnMatrixChanged() {
    switch (MATRIX_MODE) {
    case GL_MODELVIEW:
         NORMAL_DIRTY = true;
         return;
    case GL_PROJECTION:
         PROJECTION_DIRTY = true;
         return;
    case GL_TEXTURE:
         _glTnlUpdateTextureMatrix();
         return;
    case GL_COLOR:
         _glTnlUpdateColorMatrix();
         return;
    }
}


void APIENTRY glMatrixMode(GLenum mode) {
    MATRIX_MODE = mode;
    MATRIX_CUR  = MATRIX_STACKS + (mode & 0xF);
}

void APIENTRY glPushMatrix() {
    void* top = stack_top(MATRIX_CUR);
    assert(top);
    void* ret = stack_push(MATRIX_CUR, top);
    (void) ret;
    assert(ret);
    OnMatrixChanged();
}

void APIENTRY glPopMatrix() {
    stack_pop(MATRIX_CUR);
    OnMatrixChanged();
}

void APIENTRY glLoadIdentity() {
    stack_replace(MATRIX_CUR, IDENTITY);
    OnMatrixChanged();
}

void GL_FORCE_INLINE _glMultMatrix(const Matrix4x4* mat) {
    void* top = stack_top(MATRIX_CUR);

    UploadMatrix4x4(top);
    MultiplyMatrix4x4(mat);
    DownloadMatrix4x4(top);
    OnMatrixChanged();
}

void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    const Matrix4x4 trn __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, z, 1.0f
    };
    _glMultMatrix(&trn);
}


void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) {
    const Matrix4x4 scale __attribute__((aligned(32))) = {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    _glMultMatrix(&scale);
}

void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat  y, GLfloat z) {
    Matrix4x4 rotate __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

#ifdef _arch_dreamcast
    float s, c;
    fsincos(angle, &s, &c);
#else
    float r = DEG2RAD * angle;
    float c = cosf(r);
    float s = sinf(r);
#endif

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

    _glMultMatrix(&rotate);
}

/* Load an arbitrary matrix */
void APIENTRY glLoadMatrixf(const GLfloat *m) {
    static Matrix4x4 __attribute__((aligned(32))) TEMP;

    memcpy(TEMP, m, sizeof(float) * 16);
    stack_replace(MATRIX_CUR, TEMP);
    OnMatrixChanged();
}

/* Ortho */
void APIENTRY glOrtho(GLdouble left, GLdouble right,
             GLdouble bottom, GLdouble top,
             GLdouble znear, GLdouble zfar) {

    Matrix4x4 ortho __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    ortho[M0] = 2.0f / (right - left);
    ortho[M5] = 2.0f / (top - bottom);
    ortho[M10] = -2.0f / (zfar - znear);
    ortho[M12] = -(right + left) / (right - left);
    ortho[M13] = -(top + bottom) / (top - bottom);
    ortho[M14] = -(zfar + znear) / (zfar - znear);

    _glMultMatrix(&ortho);
}


/* Set the GL frustum */
void APIENTRY glFrustum(GLdouble left, GLdouble right,
               GLdouble bottom, GLdouble top,
               GLdouble znear, GLdouble zfar) {

    Matrix4x4 frustum __attribute__((aligned(32)));
    MEMSET(frustum, 0, sizeof(float) * 16);

    const float near2 = 2.0f * znear;
    const float A = (right + left) / (right - left);
    const float B = (top + bottom) / (top - bottom);
    const float C = -((zfar + znear) / (zfar - znear));
    const float D = -((2.0f * zfar * znear) / (zfar - znear));

    frustum[M0] = near2 / (right - left);
    frustum[M5] = near2 / (top - bottom);

    frustum[M8] = A;
    frustum[M9] = B;
    frustum[M10] = C;
    frustum[M11] = -1.0f;
    frustum[M14] = D;

    _glMultMatrix(&frustum);
}


/* Multiply the current matrix by an arbitrary matrix */
void glMultMatrixf(const GLfloat *m) {
    Matrix4x4 tmp __attribute__((aligned(32)));
    MEMCPY4(tmp, m, sizeof(Matrix4x4));

    _glMultMatrix(&tmp);
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

    stack_replace(MATRIX_CUR, TEMP);
    OnMatrixChanged();
}

/* Multiply the current matrix by an arbitrary transposed matrix */
void glMultTransposeMatrixf(const GLfloat *m) {
    static Matrix4x4 tmp __attribute__((aligned(32)));

    tmp[M0] = m[0];
    tmp[M1] = m[4];
    tmp[M2] = m[8];
    tmp[M3] = m[12];

    tmp[M4] = m[1];
    tmp[M5] = m[5];
    tmp[M6] = m[9];
    tmp[M7] = m[13];

    tmp[M8] = m[3];
    tmp[M9] = m[6];
    tmp[M10] = m[10];
    tmp[M11] = m[14];

    tmp[M12] = m[4];
    tmp[M13] = m[7];
    tmp[M14] = m[11];
    tmp[M15] = m[15];

    _glMultMatrix(&tmp);
}

/* Set the GL viewport */
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    VIEWPORT_MATRIX[M0]  =  width *  0.5f;
    VIEWPORT_MATRIX[M5]  = height * -0.5f;
    VIEWPORT_MATRIX[M10] = 1.0f;
    VIEWPORT_MATRIX[M15] = 1.0f;
    
    VIEWPORT_MATRIX[M12] = x + width * 0.5f;
    VIEWPORT_MATRIX[M13] = GetVideoMode()->height - (y + height * 0.5f);
    PROJECTION_DIRTY = true;
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

void APIENTRY glDepthRange(GLdouble n, GLdouble f){
    glDepthRangef(n,f);
}

/* Vector Cross Product - Used by gluLookAt */
static inline void vec3f_cross(const GLfloat* v1, const GLfloat* v2, GLfloat* result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

GL_FORCE_INLINE void vec3f_normalize_sh4(float *v){
    float lengthSq, ilength;

    lengthSq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    ilength  = MATH_fsrra(lengthSq);

    if (lengthSq)
    {
        v[0] *= ilength;
        v[1] *= ilength;
        v[2] *= ilength;
    }
}

void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble centerx,
               GLdouble centery, GLdouble centerz, GLdouble upx, GLdouble upy,
               GLdouble upz) {
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

void _glMatrixLoadModelView() {
    UploadMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadProjection() {
    if (PROJECTION_DIRTY) UpdateProjectionMatrix();
    UploadMatrix4x4(&PROJECTION_MATRIX);
}

void _glMatrixLoadModelViewProjection() {
    if (PROJECTION_DIRTY) UpdateProjectionMatrix();
    UploadMatrix4x4(&PROJECTION_MATRIX);
    MultiplyMatrix4x4((const Matrix4x4*) stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadNormal() {
    if (NORMAL_DIRTY) UpdateNormalMatrix();
    UploadMatrix4x4((const Matrix4x4*) &NORMAL_MATRIX);
}
