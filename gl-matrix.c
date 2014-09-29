/* KallistiGL for KallistiOS ##version##

   libgl/gl-matrix.c
   Copyright (C) 2013-2014 Josh Pearson
   Copyright (C) 2014 Lawrence Sebald

   Some functionality adapted from the original KOS libgl:
   Copyright (C) 2001 Dan Potter

   The GL matrix operations use the KOS SH4 matrix operations.
   Basically, we keep two seperate matrix stacks:
   1.) Internal GL API Matrix Stack ( screenview, modelview, etc. ) ( fixed stack size )
   2.) External Matrix Stack for client to push / pop ( size of each stack is determined by MAX_MATRICES )
*/

#include <string.h>

#include "gl.h"
#include "glu.h"
#include "gl-api.h"
#include "gl-sh4.h"

/* This Matrix contains the GL Base Stack */
static matrix4f Matrix[GL_MATRIX_COUNT] __attribute__((aligned(32)));
static GLsizei MatrixMode = 0;

/* This Matrix contains the GL Push/Pop Stack ( fixed size per mode, 32 matrices )*/
static const GLsizei MAX_MATRICES = 32;
static matrix4f MatrixStack[GL_MATRIX_COUNT][32] __attribute__((aligned(32)));
static GLsizei  MatrixStackPos[GL_MATRIX_COUNT];

/* Viewport mapping */
static GLfloat gl_viewport_scale[3], gl_viewport_offset[3];

/* Depth range */
static GLclampf gl_depthrange_near, gl_depthrange_far;

/* Viewport size */
static GLint gl_viewport_x1, gl_viewport_y1, gl_viewport_width, gl_viewport_height;

/* Frustum attributes */
typedef struct {
    float left, right, bottom, top, znear, zfar;
} gl_frustum_t;

static gl_frustum_t gl_frustum;

/* Frustum Matrix */
static matrix4f FrustumMatrix __attribute__((aligned(32))) = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};

/* Ortho Matrix */
static matrix4f OrthoMatrix __attribute__((aligned(32))) = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* Matrix for user to submit externally, ensure 32byte allignment */
static matrix4f ml __attribute__((aligned(32)));

/* Look-At Matrix */
static matrix4f MatrixLookAt __attribute__((aligned(32))) = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* Modelview Rotation Matrix - Applied to Vertex Normal when Lighting is Enabled */
static matrix4f MatrixMdlRot __attribute__((aligned(32))) = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};

void glMatrixMode(GLenum mode) {
    if(mode >= GL_SCREENVIEW && mode <= GL_IDENTITY)
        MatrixMode = mode;
}

void glPushMatrix() {
    if(MatrixStackPos[MatrixMode] < MAX_MATRICES - 1) {
        mat_load(Matrix + MatrixMode);
        mat_store(&MatrixStack[MatrixMode][MatrixStackPos[MatrixMode]]);
        ++MatrixStackPos[MatrixMode];
    }
}

void glPopMatrix() {
    if(MatrixStackPos[MatrixMode]) {
        --MatrixStackPos[MatrixMode];
        mat_load(&MatrixStack[MatrixMode][MatrixStackPos[MatrixMode]]);
        mat_store(Matrix + MatrixMode);
    }
}

void glLoadIdentity() {
    mat_load(Matrix + GL_IDENTITY);
    mat_store(Matrix + MatrixMode);

    if(MatrixMode == GL_MODELVIEW)
        mat_store(&MatrixMdlRot);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    mat_load(Matrix + MatrixMode);
    mat_translate(x, y, z);
    mat_store(Matrix + MatrixMode);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    mat_load(Matrix + MatrixMode);
    mat_scale(x, y, z);
    mat_store(Matrix + MatrixMode);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat  y, GLfloat z) {
    float r = DEG2RAD * angle;
    mat_load(Matrix + MatrixMode);
    mat_rotate(r * x, r * y, r * z);
    mat_store(Matrix + MatrixMode);

    if(MatrixMode == GL_MODELVIEW) {
        mat_load(&MatrixMdlRot);
        mat_rotate(r * x, r * y, r * z);
        mat_store(&MatrixMdlRot);
    }
}

/* Load an arbitrary matrix */
void glLoadMatrixf(const GLfloat *m) {
    memcpy(ml, m, sizeof(matrix4f));
    mat_load(&ml);
    mat_store(Matrix + MatrixMode);
}

/* Load an arbitrary transposed matrix */
void glLoadTransposeMatrixf(const GLfloat *m) {
    ml[0][0] = m[0];
    ml[0][1] = m[4];
    ml[0][2] = m[8];
    ml[0][3] = m[12];
    ml[1][0] = m[1];
    ml[1][1] = m[5];
    ml[1][2] = m[9];
    ml[1][3] = m[13];
    ml[2][0] = m[2];
    ml[2][1] = m[6];
    ml[2][2] = m[10];
    ml[2][3] = m[14];
    ml[3][0] = m[3];
    ml[3][1] = m[7];
    ml[3][2] = m[11];
    ml[3][3] = m[15];

    mat_load(&ml);
    mat_store(Matrix + MatrixMode);
}

/* Multiply the current matrix by an arbitrary matrix */
void glMultMatrixf(const GLfloat *m) {
    memcpy(ml, m, sizeof(matrix4f));
    mat_load(Matrix + MatrixMode);
    mat_apply(&ml);
    mat_store(Matrix + MatrixMode);
}

/* Multiply the current matrix by an arbitrary transposed matrix */
void glMultTransposeMatrixf(const GLfloat *m) {
    ml[0][0] = m[0];
    ml[0][1] = m[4];
    ml[0][2] = m[8];
    ml[0][3] = m[12];
    ml[1][0] = m[1];
    ml[1][1] = m[5];
    ml[1][2] = m[9];
    ml[1][3] = m[13];
    ml[2][0] = m[2];
    ml[2][1] = m[6];
    ml[2][2] = m[10];
    ml[2][3] = m[14];
    ml[3][0] = m[3];
    ml[3][1] = m[7];
    ml[3][2] = m[11];
    ml[3][3] = m[15];

    mat_load(Matrix + MatrixMode);
    mat_apply(&ml);
    mat_store(Matrix + MatrixMode);
}

/* Set the depth range */
void glDepthRange(GLclampf n, GLclampf f) {
    /* clamp the values... */
    if(n < 0.0f) n = 0.0f;
    else if(n > 1.0f) n = 1.0f;

    if(f < 0.0f) f = 0.0f;
    else if(f > 1.0f) f = 1.0f;

    gl_depthrange_near = n;
    gl_depthrange_far = f;

    /* Adjust the viewport scale and offset for Z */
    gl_viewport_scale[2] = ((f - n) / 2.0f);
    gl_viewport_offset[2] = (n + f) / 2.0f;
}

/* Set the GL viewport */
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    gl_viewport_x1 = x;
    gl_viewport_y1 = y;
    gl_viewport_width = width;
    gl_viewport_height = height;

    /* Calculate the viewport scale and offset */
    gl_viewport_scale[0] = (GLfloat)width / 2.0f;
    gl_viewport_offset[0] = gl_viewport_scale[0] + (GLfloat)x;
    gl_viewport_scale[1] = (GLfloat)height / 2.0f;
    gl_viewport_offset[1] = gl_viewport_scale[1] + (GLfloat)y;
    gl_viewport_scale[2] = (gl_depthrange_far - gl_depthrange_near) / 2.0f;
    gl_viewport_offset[2] = (gl_depthrange_near + gl_depthrange_far) / 2.0f;

    gl_viewport_offset[2] += 0.0001f;

    /* Set the Screenview Matrix based on the viewport */
    Matrix[GL_SCREENVIEW][0][0] = gl_viewport_scale[0];
    Matrix[GL_SCREENVIEW][1][1] = -gl_viewport_scale[1];
    Matrix[GL_SCREENVIEW][2][2] = 1;
    Matrix[GL_SCREENVIEW][3][0] = gl_viewport_offset[0];
    Matrix[GL_SCREENVIEW][3][1] = vid_mode->height - gl_viewport_offset[1];
}

/* Set the GL frustum */
void glFrustum(GLfloat left, GLfloat right,
               GLfloat bottom, GLfloat top,
               GLfloat znear, GLfloat zfar) {
    gl_frustum.left = left;
    gl_frustum.right = right;
    gl_frustum.bottom = bottom;
    gl_frustum.top = top;
    gl_frustum.znear = znear;
    gl_frustum.zfar = zfar;

    FrustumMatrix[0][0] = (2.0f * znear) / (right - left);
    FrustumMatrix[2][0] = (right + left) / (right - left);
    FrustumMatrix[1][1] = (2.0f * znear) / (top - bottom);
    FrustumMatrix[2][1] = (top + bottom) / (top - bottom);
    FrustumMatrix[2][2] = zfar / (zfar - znear);
    FrustumMatrix[3][2] = -(zfar * znear) / (zfar - znear);

    mat_load(Matrix + MatrixMode);
    mat_apply(&FrustumMatrix);
    mat_store(Matrix + MatrixMode);
}

/* Ortho */
void glOrtho(GLfloat left, GLfloat right,
             GLfloat bottom, GLfloat top,
             GLfloat znear, GLfloat zfar) {
    OrthoMatrix[0][0] = 2.0f / (right - left);
    OrthoMatrix[1][1] = 2.0f / (top - bottom);
    OrthoMatrix[2][2] = -2.0f / (zfar - znear);
    OrthoMatrix[3][0] = -(right + left) / (right - left);;
    OrthoMatrix[3][1] = -(top + bottom) / (top - bottom);
    OrthoMatrix[3][2] = -(zfar + znear) / (zfar - znear);

    mat_load(Matrix + MatrixMode);
    mat_apply(&OrthoMatrix);
    mat_store(Matrix + MatrixMode);
}

/* Set the Perspective */
void gluPerspective(GLfloat angle, GLfloat aspect,
                    GLfloat znear, GLfloat zfar) {
    GLfloat xmin, xmax, ymin, ymax;

    ymax = znear * ftan(angle * F_PI / 360.0f);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustum(xmin, xmax, ymin, ymax, znear, zfar);
}

/* Vector Cross Product - Used by glhLookAtf2 */
void vec3f_cross(vector3f v1, vector3f v2, vector3f result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/* glhLookAtf2 adapted from http://www.opengl.org/wiki/GluLookAt_code */
void glhLookAtf2(vector3f eyePosition3D,
                 vector3f center3D,
                 vector3f upVector3D) {
    vector3f forward, side, up;

    _glKosSetEyePosition(eyePosition3D);

    vec3f_sub_normalize(center3D[0], center3D[1], center3D[2],
                        eyePosition3D[0], eyePosition3D[1], eyePosition3D[2],
                        forward[0], forward[1], forward[2]);

    //Side = forward x up
    vec3f_cross(forward, upVector3D, side);
    vec3f_normalize(side[0], side[1], side[2]);

    //Recompute up as: up = side x forward
    vec3f_cross(side, forward, up);

    MatrixLookAt[0][0] = side[0];
    MatrixLookAt[1][0] = side[1];
    MatrixLookAt[2][0] = side[2];
    MatrixLookAt[3][0] = 0;

    MatrixLookAt[0][1] = up[0];
    MatrixLookAt[1][1] = up[1];
    MatrixLookAt[2][1] = up[2];
    MatrixLookAt[3][1] = 0;

    MatrixLookAt[0][2] = -forward[0];
    MatrixLookAt[1][2] = -forward[1];
    MatrixLookAt[2][2] = -forward[2];
    MatrixLookAt[3][2] = 0;

    MatrixLookAt[0][3] =
        MatrixLookAt[1][3] =
            MatrixLookAt[2][3] = 0;
    MatrixLookAt[3][3] = 1;

    // Does not modify internal Modelview matrix
    mat_load(&MatrixLookAt);
    mat_translate(-eyePosition3D[0], -eyePosition3D[1], -eyePosition3D[2]);
    mat_store(&MatrixLookAt);
}

void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx,
               GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy,
               GLfloat upz) {
    vector3f eye = { eyex, eyey, eyez };
    vector3f point = { centerx, centery, centerz };
    vector3f up = { upx, upy, upz };
    glhLookAtf2(eye, point, up);
}

void _glKosMatrixApplyRender() {
    mat_load(Matrix + GL_SCREENVIEW);
    mat_apply(Matrix + GL_PROJECTION);
    mat_apply(&MatrixLookAt);
    mat_apply(Matrix + GL_MODELVIEW);
    mat_store(Matrix + GL_RENDER);
}

void _glKosMatrixLoadRender() {
    mat_load(Matrix + GL_RENDER);
}

void _glKosMatrixLoadModelView() {
    mat_load(Matrix + GL_MODELVIEW);
}

void _glKosMatrixLoadModelRot() {
    mat_load(&MatrixMdlRot);
}

void _glKosMatrixApplyScreenSpace() {
    mat_load(Matrix + GL_SCREENVIEW);
    mat_apply(Matrix + GL_PROJECTION);
    mat_apply(&MatrixLookAt);
}

void _glKosInitMatrix() {
    mat_identity();
    mat_store(Matrix + GL_SCREENVIEW);
    mat_store(Matrix + GL_PROJECTION);
    mat_store(Matrix + GL_MODELVIEW);
    mat_store(Matrix + GL_TEXTURE);
    mat_store(Matrix + GL_IDENTITY);
    mat_store(Matrix + GL_RENDER);

    int i;

    for(i = 0; i < GL_MATRIX_COUNT; i++)
        MatrixStackPos[i] = 0;

    glDepthRange(0.0f, 1.0f);
    glViewport(0, 0, vid_mode->width, vid_mode->height);
}

void glKosGetMatrix(GLenum mode, GLfloat *params) {
    if(mode < GL_SCREENVIEW || mode > GL_RENDER)
        *params = (GLfloat)GL_INVALID_ENUM;

    memcpy(params, Matrix + mode, sizeof(GLfloat) * 16);
}
