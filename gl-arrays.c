/* KallistiGL for KallistiOS ##version##

   libgl/gl-arrays.c
   Copyright (C) 2013-2015 Josh Pearson

   Arrays Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS

   Here, it is not necessary to enable or disable client states;
   the API is aware of what pointers have been submitted, and will
   render accordingly.  If you submit a normal pointer, dynamic
   vertex lighting will be applied even if you submit a color
   pointer, so only submit one or the other.
*/

#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "glext.h"
#include "gl-api.h"
#include "gl-arrays.h"
#include "gl-pvr.h"
#include "gl-rgb.h"
#include "gl-sh4.h"

//========================================================================================//
//== Local Variables ==//

static glVertex  GL_KOS_ARRAY_BUF[GL_KOS_MAX_VERTS];
static glVertex *GL_KOS_ARRAY_BUF_PTR;

static GLfloat   GL_KOS_ARRAY_BUFW[GL_KOS_MAX_VERTS];
static GLfloat   GL_KOS_ARRAY_DSTW[GL_KOS_MAX_VERTS];

static GLfloat   GL_KOS_ARRAY_BUFUV[GL_KOS_MAX_VERTS];

static GLubyte GL_KOS_CLIENT_ACTIVE_TEXTURE = GL_TEXTURE0_ARB & 0xF;

static GLubyte  *GL_KOS_VERTEX_POINTER = NULL;
static GLubyte  *GL_KOS_NORMAL_POINTER = NULL;
static GLubyte  *GL_KOS_TEXCOORD0_POINTER = NULL;
static GLubyte  *GL_KOS_TEXCOORD1_POINTER = NULL;
static GLubyte  *GL_KOS_COLOR_POINTER = NULL;
static GLubyte  *GL_KOS_INDEX_POINTER_U8 = NULL;
static GLushort *GL_KOS_INDEX_POINTER_U16 = NULL;

static GLushort GL_KOS_VERTEX_STRIDE = 0;
static GLushort GL_KOS_NORMAL_STRIDE = 0;
static GLushort GL_KOS_TEXCOORD0_STRIDE = 0;
static GLushort GL_KOS_TEXCOORD1_STRIDE = 0;
static GLushort GL_KOS_COLOR_STRIDE = 0;

static GLuint  GL_KOS_VERTEX_PTR_MODE = 0;
static GLubyte GL_KOS_VERTEX_SIZE = 0;
static GLubyte GL_KOS_COLOR_COMPONENTS = 0;
static GLenum  GL_KOS_COLOR_TYPE = 0;

//========================================================================================//
//== Local Function Definitions ==//

static inline void _glKosArraysTransformNormals(GLubyte *normal, GLuint count);
static inline void _glKosArraysTransformPositions(GLubyte *position, GLuint count);

void (*_glKosArrayTexCoordFunc)(pvr_vertex_t *);
void (*_glKosArrayColorFunc)(pvr_vertex_t *);

void (*_glKosElementTexCoordFunc)(pvr_vertex_t *, GLuint);
void (*_glKosElementColorFunc)(pvr_vertex_t *, GLuint);

static inline GLushort _calculate_byte_size(GLenum type) {
    switch(type) {
        case GL_UNSIGNED_INT: return sizeof(GLuint);
        case GL_INT: return sizeof(GLint);
        case GL_FLOAT: return sizeof(GLfloat);
        case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
        case GL_BYTE: return sizeof(GLbyte);
        case GL_UNSIGNED_SHORT: return sizeof(GLushort);
        case GL_SHORT: return sizeof(GLshort);
        case GL_DOUBLE: return sizeof(GLdouble);
        default:
            _glKosThrowError(GL_INVALID_ENUM, "_calculate_byte_size");
            return 0;
    }
}

//========================================================================================//
//== Open GL API Public Functions ==//

/* Submit a Vertex Position Pointer */
//GLAPI void APIENTRY glVertexPointer(GLint size, GLenum type,
//                                    GLsizei stride, const GLvoid *pointer) {
//    if(size != 2) /* Expect 2D X,Y or 3D X,Y,Z vertex... */
//        if(size != 3)
//            _glKosThrowError(GL_INVALID_VALUE, "glVertexPointer");

//    if(type != GL_FLOAT) /* Expect Floating point vertices */
//        _glKosThrowError(GL_INVALID_ENUM, "glVertexPointer");

//    if(stride < 0)
//        _glKosThrowError(GL_INVALID_VALUE, "glVertexPointer");

//    if(_glKosHasError()) {
//        _glKosPrintError();
//        return;
//    }

//    GL_KOS_VERTEX_SIZE = size;

//    GL_KOS_VERTEX_STRIDE = (stride) ? stride : _calculate_byte_size(type) * size;
//    GL_KOS_VERTEX_POINTER = (GLubyte *)pointer;
//}

///* Submit a Vertex Normal Pointer */
//GLAPI void APIENTRY glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {
//    if(type != GL_FLOAT) /* Expect Floating point vertices */
//        _glKosThrowError(GL_INVALID_ENUM, "glNormalPointer");

//    if(stride < 0)
//        _glKosThrowError(GL_INVALID_VALUE, "glNormalPointer");

//    if(_glKosHasError()) {
//        _glKosPrintError();
//        return;
//    }

//    GL_KOS_NORMAL_STRIDE = (stride) ? stride : _calculate_byte_size(type) * 3;
//    GL_KOS_NORMAL_POINTER = (GLubyte *)pointer;
//}

///* Submit a Texture Coordinate Pointer */
//GLAPI void APIENTRY glTexCoordPointer(GLint size, GLenum type,
//                                      GLsizei stride, const GLvoid *pointer) {
//    if(size != 2)  /* Expect u and v */
//        _glKosThrowError(GL_INVALID_VALUE, "glTexCoordPointer");

//    if(type != GL_FLOAT) /* Expect Floating point vertices */
//        _glKosThrowError(GL_INVALID_ENUM, "glTexCoordPointer");

//    if(stride < 0)
//        _glKosThrowError(GL_INVALID_VALUE, "glTexCoordPointer");

//    if(_glKosHasError()) {
//        _glKosPrintError();
//        return;
//    }

//    if(GL_KOS_CLIENT_ACTIVE_TEXTURE) {
//        GL_KOS_TEXCOORD1_STRIDE = (stride) ? stride : _calculate_byte_size(type) * size;
//        GL_KOS_TEXCOORD1_POINTER = (GLubyte *)pointer;
//    }
//    else {
//        GL_KOS_TEXCOORD0_STRIDE = (stride) ? stride : _calculate_byte_size(type) * size;
//        GL_KOS_TEXCOORD0_POINTER = (GLubyte *)pointer;
//    }
//}

///* Submit a Color Pointer */
//GLAPI void APIENTRY glColorPointer(GLint size, GLenum type,
//                                   GLsizei stride, const GLvoid *pointer) {
//    if((type == GL_UNSIGNED_INT) && (size == 1)) {
//        GL_KOS_COLOR_COMPONENTS = size;
//        GL_KOS_COLOR_POINTER = (GLubyte *)pointer;
//        GL_KOS_COLOR_TYPE = type;
//    }
//    else if((type == GL_UNSIGNED_BYTE) && (size == 4)) {
//        GL_KOS_COLOR_COMPONENTS = size;
//        GL_KOS_COLOR_POINTER = (GLubyte *)pointer;
//        GL_KOS_COLOR_TYPE = type;
//    }
//    else if((type == GL_FLOAT) && (size == 3)) {
//        GL_KOS_COLOR_COMPONENTS = size;
//        GL_KOS_COLOR_POINTER = (GLubyte *)pointer;
//        GL_KOS_COLOR_TYPE = type;
//    }
//    else if((type == GL_FLOAT) && (size == 4)) {
//        GL_KOS_COLOR_COMPONENTS = size;
//        GL_KOS_COLOR_POINTER = (GLubyte *)pointer;
//        GL_KOS_COLOR_TYPE = type;
//    }
//    else {
//        _glKosThrowError(GL_INVALID_ENUM, "glColorPointer");
//        _glKosPrintError();
//        return;
//    }

//    GL_KOS_COLOR_STRIDE = (stride) ? stride : _calculate_byte_size(type) * size;
//}
//========================================================================================//
//== Vertex Pointer Internal API ==//

inline void _glKosArrayBufIncrement() {
    ++GL_KOS_ARRAY_BUF_PTR;
}

inline void _glKosArrayBufReset() {
    GL_KOS_ARRAY_BUF_PTR = &GL_KOS_ARRAY_BUF[0];
}

inline glVertex *_glKosArrayBufAddr() {
    return &GL_KOS_ARRAY_BUF[0];
}

inline glVertex *_glKosArrayBufPtr() {
    return GL_KOS_ARRAY_BUF_PTR;
}

static inline void _glKosArraysTransformNormals(GLubyte *normal, GLuint count) {
    glVertex *v = &GL_KOS_ARRAY_BUF[0];
    GLubyte *src = normal;
    GLfloat *N;

    _glKosMatrixLoadModelRot();

    while(count--) {
        N = (GLfloat *) src;
        mat_trans_normal3_nomod(N[0], N[1], N[2], v->norm[0], v->norm[1], v->norm[2]);
        src += GL_KOS_NORMAL_STRIDE;
        ++v;
    }
}

static inline void _glKosArraysTransformPositions(GLubyte *position, GLuint count) {
    glVertex *v = &GL_KOS_ARRAY_BUF[0];
    GLubyte *src = position;
    GLfloat *P;

    _glKosMatrixLoadModelView();

    while(count--) {
        P = (GLfloat *) src;
        mat_trans_single3_nodiv_nomod(P[0], P[1], P[2], v->pos[0], v->pos[1], v->pos[2]);
        src += GL_KOS_VERTEX_STRIDE;
        ++v;
    }
}

//========================================================================================//
//== Arrays Vertex Transform ==/
static void _glKosArraysTransform2D(GLuint count) {
    GLubyte *src = GL_KOS_VERTEX_POINTER;
    GLfloat *element;
    pvr_vertex_t *dst = _glKosVertexBufPointer();

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    while(count--)  {
        element = (GLfloat *) src;
        __x = element[0];
        __y = element[1];
        __z = 0;

        mat_trans_fv12()

        dst->x = __x;
        dst->y = __y;
        dst->z = __z;

        ++dst;

        src += GL_KOS_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransform(GLuint count) {
    GLubyte *src = GL_KOS_VERTEX_POINTER;
    GLfloat *element;
    pvr_vertex_t *dst = _glKosVertexBufPointer();

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    while(count--)  {
        element = (GLfloat *) src;
        __x = element[0];
        __y = element[1];
        __z = element[2];

        mat_trans_fv12()

        dst->x = __x;
        dst->y = __y;
        dst->z = __z;

        ++dst;

        src += GL_KOS_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransformClip(GLuint count) {
    GLubyte *src = GL_KOS_VERTEX_POINTER;
    GLfloat *element;
    GLfloat *W = GL_KOS_ARRAY_DSTW;
    pvr_vertex_t *dst = _glKosClipBufAddress();

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");
    register float __w  __asm__("fr15");

    while(count--)  {
        element = (GLfloat *) src;
        __x = element[0];
        __y = element[1];
        __z = element[2];

        mat_trans_fv12_nodivw()

        dst->x = __x;
        dst->y = __y;
        dst->z = __z;
        *W++ = __w;

        ++dst;

        src += GL_KOS_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransformElements(GLuint count) {
    GLubyte *src = GL_KOS_VERTEX_POINTER;
    GLfloat *element;
    GLuint i = 0;

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    for(i = 0; i < count; i++) {
        element = (GLfloat *) src;
        __x = element[0];
        __y = element[1];
        __z = element[2];

        mat_trans_fv12()

        GL_KOS_ARRAY_BUF[i].pos[0] = __x;
        GL_KOS_ARRAY_BUF[i].pos[1] = __y;
        GL_KOS_ARRAY_BUF[i].pos[2] = __z;

        src += GL_KOS_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransformClipElements(GLuint count) {
    GLubyte *src = GL_KOS_VERTEX_POINTER;
    GLfloat *element;
    GLuint i;

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");
    register float __w  __asm__("fr15");

    for(i = 0; i < count; i++) {
        element = (GLfloat *) src;
        __x = element[0];
        __y = element[1];
        __z = element[2];

        mat_trans_fv12_nodivw()

        GL_KOS_ARRAY_BUF[i].pos[0] = __x;
        GL_KOS_ARRAY_BUF[i].pos[1] = __y;
        GL_KOS_ARRAY_BUF[i].pos[2] = __z;
        GL_KOS_ARRAY_BUFW[i] = __w;

        src += GL_KOS_VERTEX_STRIDE;
    }
}

//========================================================================================//
//== Element Attribute Functions ==//

//== Color ==//

static inline void _glKosArrayColor0(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    dst[0].argb = _glKosVertexColor();

    for(i = 1; i < count; i++)
        dst[i].argb = dst[0].argb;
}

static inline void _glKosElementColor1uiU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *element;
    GLubyte *src = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        element = (GLuint *) (src + (GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_COLOR_STRIDE));
        dst[i].argb = *element;
    }
}

static inline void _glKosElementColor1uiU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        element = (GLuint *) (color + (GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_COLOR_STRIDE));
        dst[i].argb = *element;
    }
}

static inline void _glKosElementColor4ubU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        element = (GLuint *) (color + (GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_COLOR_STRIDE));
        dst[i].argb = RGBA32_2_ARGB32(*element);
    }
}

static inline void _glKosElementColor4ubU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        element = (GLuint *) (color + (GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_COLOR_STRIDE));
        dst[i].argb = RGBA32_2_ARGB32(*element);
    }
}

static inline void _glKosElementColor3fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgb3f* element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_COLOR_STRIDE;
        element = (GLrgb3f *) (color + index);
        dst[i].argb = (0xFF000000
                       | ((GLubyte)((*element)[0] * 0xFF)) << 16
                       | ((GLubyte)((*element)[1] * 0xFF)) << 8
                       | ((GLubyte)((*element)[2] * 0xFF)));
    }
}

static inline void _glKosElementColor3fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgb3f* element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index =  GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_COLOR_STRIDE;
        element = (GLrgb3f *) (color + index);
        dst[i].argb = (0xFF000000
                       | ((GLubyte)((*element)[0] * 0xFF)) << 16
                       | ((GLubyte)((*element)[1] * 0xFF)) << 8
                       | ((GLubyte)((*element)[2] * 0xFF)));
    }
}

static inline void _glKosElementColor4fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgba4f* element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_COLOR_STRIDE;
        element = (GLrgba4f *) (color + index);

        dst[i].argb = (  ((GLubyte) ((*element)[3] * 0xFF)) << 24
                       | ((GLubyte) ((*element)[0] * 0xFF)) << 16
                       | ((GLubyte) ((*element)[1] * 0xFF)) << 8
                       | ((GLubyte) ((*element)[2] * 0xFF)));
    }
}

static inline void _glKosElementColor4fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgba4f* element;
    GLubyte* color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_COLOR_STRIDE;
        element = (GLrgba4f *) (color + index);
        dst[i].argb = (  ((GLubyte)((*element)[3] * 0xFF)) << 24
                       | ((GLubyte)((*element)[0] * 0xFF)) << 16
                       | ((GLubyte)((*element)[1] * 0xFF)) << 8
                       | ((GLubyte)((*element)[2] * 0xFF)));
    }
}

//== Texture Coordinates ==//



static inline void _glKosElementTexCoord2fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD0_POINTER;
    GLfloat *t;

    if(_glKosEnabledTextureMatrix()) {
        _glKosMatrixLoadTexture();

        for(i = 0; i < count; i++) {
            index = GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_TEXCOORD0_STRIDE;
            t = (GLfloat *) (src + index);
            mat_trans_texture2_nomod(t[0], t[1], dst[i].u, dst[i].v);
        }

        _glKosMatrixLoadRender();
    }
    else {
        for(i = 0; i < count; i++) {
            index = GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_TEXCOORD0_STRIDE;
            t = (GLfloat *) (src + index);
            dst[i].u = t[0];
            dst[i].v = t[1];
        }
    }
}

static inline void _glKosElementTexCoord2fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD0_POINTER;
    GLfloat *t;

    if(_glKosEnabledTextureMatrix()) {
        _glKosMatrixLoadTexture();

        for(i = 0; i < count; i++) {
            index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_TEXCOORD0_STRIDE;
            t = (GLfloat *) (src + index);
            mat_trans_texture2_nomod(t[0], t[1], dst[i].u, dst[i].v);
        }

        _glKosMatrixLoadRender();
    }
    else {
        for(i = 0; i < count; i++) {
            index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_TEXCOORD0_STRIDE;
            t = (GLfloat *) (src + index);
            dst[i].u = t[0];
            dst[i].v = t[1];
        }
    }
}

static inline void _glKosElementMultiTexCoord2fU16C(GLuint count) {
    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD1_POINTER;
    GLfloat *t;
    GLfloat *dst = GL_KOS_ARRAY_BUFUV;

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_TEXCOORD1_STRIDE;
        t = (GLfloat *) (src + index);
        *dst++ = t[0];
        *dst++ = t[1];
    }
}

static inline void _glKosElementMultiTexCoord2fU8C(GLuint count) {
    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD1_POINTER;
    GLfloat *t;
    GLfloat *dst = GL_KOS_ARRAY_BUFUV;

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_TEXCOORD1_STRIDE;
        t = (GLfloat *) (src + index);
        *dst++ = t[0];
        *dst++ = t[1];
    }
}

static inline void _glKosElementMultiTexCoord2fU16(GLuint count) {
    if(_glKosEnabledNearZClip())
        return _glKosElementMultiTexCoord2fU16C(count);

    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD1_POINTER;
    GLfloat *t;
    glTexCoord *dst = (glTexCoord *)_glKosMultiUVBufPointer();

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U16[i] * GL_KOS_TEXCOORD1_STRIDE;
        t = (GLfloat *) (src + index);
        dst[i].u = t[0];
        dst[i].v = t[1];
    }

    _glKosMultiUVBufAdd(count);
}

static inline void _glKosElementMultiTexCoord2fU8(GLuint count) {
    if(_glKosEnabledNearZClip())
        return _glKosElementMultiTexCoord2fU8C(count);

    GLuint i, index;
    GLubyte *src = GL_KOS_TEXCOORD1_POINTER;
    GLfloat *t;
    glTexCoord *dst = (glTexCoord *)_glKosMultiUVBufPointer();

    for(i = 0; i < count; i++) {
        index = GL_KOS_INDEX_POINTER_U8[i] * GL_KOS_TEXCOORD1_STRIDE;
        t = (GLfloat *) (src + index);
        dst[i].u = t[0];
        dst[i].v = t[1];
    }

    _glKosMultiUVBufAdd(count);
}

//========================================================================================//
//== Element Unpacking ==//

static inline void _glKosArraysUnpackElementsS16(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_KOS_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[0];
        dst[i].y = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[1];
        dst[i].z = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[2];
    }
}

static inline void _glKosArraysUnpackElementsS8(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_KOS_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[0];
        dst[i].y = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[1];
        dst[i].z = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[2];
    }
}

static inline void _glKosArraysUnpackClipElementsS16(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_KOS_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[0];
        dst[i].y = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[1];
        dst[i].z = vert[GL_KOS_INDEX_POINTER_U16[i]].pos[2];
        GL_KOS_ARRAY_DSTW[i] = GL_KOS_ARRAY_BUFW[GL_KOS_INDEX_POINTER_U16[i]];
    }
}

static inline void _glKosArraysUnpackClipElementsS8(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_KOS_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[0];
        dst[i].y = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[1];
        dst[i].z = vert[GL_KOS_INDEX_POINTER_U8[i]].pos[2];
        GL_KOS_ARRAY_DSTW[i] = GL_KOS_ARRAY_BUFW[GL_KOS_INDEX_POINTER_U8[i]];
    }
}

//========================================================================================//
//== Misc Utils ==//

static inline void _glKosVertexSwizzle(pvr_vertex_t *v1, pvr_vertex_t *v2) {
    pvr_vertex_t tmp = *v1;
    *v1 = *v2;
    *v2 = * &tmp;
}

static inline void _glKosTexCoordSwizzle(glTexCoord *uv1, glTexCoord *uv2) {
    glTexCoord tmp = *uv1;
    *uv1 = *uv2;
    *uv2 = * &tmp;
}


//========================================================================================//
//== Vertex Flag Settings for the PVR2DC hardware ==//

static inline void _glKosArrayFlagsSetQuad(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i += 4) {
        _glKosVertexSwizzle(&dst[i + 2], &dst[i + 3]);
        dst[i + 0].flags = dst[i + 1].flags = dst[i + 2].flags = PVR_CMD_VERTEX;
        dst[i + 3].flags = PVR_CMD_VERTEX_EOL;
    }
}

static inline void _glKosArrayFlagsSetTriangle(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i += 3) {
        dst[i + 0].flags = dst[i + 1].flags = PVR_CMD_VERTEX;
        dst[i + 2].flags = PVR_CMD_VERTEX_EOL;
    }
}

static inline void _glKosArrayFlagsSetTriangleStrip(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count - 1; i++)
        dst[i].flags = PVR_CMD_VERTEX;

    dst[i].flags = PVR_CMD_VERTEX_EOL;
}


static inline void _glKosArraysSwizzleQuadsMultiTex(GLuint count) {
    if(!_glKosEnabledNearZClip()) {
        GLuint i;
        glTexCoord *t = (glTexCoord *)_glKosMultiUVBufPointer() - count;

        for(i = 0; i < count; i += 4)
            _glKosTexCoordSwizzle(&t[i + 2], &t[i + 3]);
    }
}

//========================================================================================//
//== OpenGL Error Code Generation ==//

static GLuint _glKosArraysVerifyParameter(GLenum mode, GLsizei count, GLenum type, GLubyte element) {
    if(mode != GL_QUADS)
        if(mode != GL_TRIANGLES)
            if(mode != GL_TRIANGLE_STRIP)
                _glKosThrowError(GL_INVALID_ENUM, (element) ? "glDrawElements" : "glDrawArrays");

    if(count < 0)
        _glKosThrowError(GL_INVALID_VALUE, (element) ? "glDrawElements" : "glDrawArrays");

    if(count > GL_KOS_MAX_VERTS)
        _glKosThrowError(GL_OUT_OF_MEMORY, (element) ? "glDrawElements" : "glDrawArrays");

    if(element) {
        switch(type) {
            case GL_UNSIGNED_BYTE:
            case GL_UNSIGNED_SHORT:
                break;

            default:
                _glKosThrowError(GL_INVALID_ENUM, (element) ? "glDrawElements" : "glDrawArrays");
        }
    }
    else if(type > count)
        _glKosThrowError(GL_INVALID_VALUE, (element) ? "glDrawElements" : "glDrawArrays");

    if(_glKosHasError()) {
        _glKosPrintError();
        return 0;
    }

    return (GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_ARRAY) ? 1 : 0;
}

static GLuint _glKosArraysApplyClipping(GLfloat *uvsrc, GLuint uvstride, GLenum mode, GLuint count) {
    switch(mode) {
        case GL_TRIANGLES:
            if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE1) {
                count = _glKosClipTrianglesTransformedMT((pvr_vertex_t *)_glKosClipBufAddress(),
                        GL_KOS_ARRAY_DSTW,
                        (pvr_vertex_t *)_glKosVertexBufPointer(),
                        uvsrc,
                        (glTexCoord *)_glKosMultiUVBufPointer(),
                        uvstride,
                        count);
                _glKosMultiUVBufAdd(count);
            }
            else
                count = _glKosClipTrianglesTransformed((pvr_vertex_t *)_glKosClipBufAddress(),
                                                       GL_KOS_ARRAY_DSTW,
                                                       (pvr_vertex_t *)_glKosVertexBufPointer(),
                                                       count);

            break;

        case GL_QUADS:
            if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE1) {
                count = _glKosClipQuadsTransformedMT((pvr_vertex_t *)_glKosClipBufAddress(),
                                                     GL_KOS_ARRAY_DSTW,
                                                     (pvr_vertex_t *)_glKosVertexBufPointer(),
                                                     uvsrc,
                                                     (glTexCoord *)_glKosMultiUVBufPointer(),
                                                     uvstride,
                                                     count);
                _glKosMultiUVBufAdd(count);
            }
            else
                count = _glKosClipQuadsTransformed((pvr_vertex_t *)_glKosClipBufAddress(),
                                                   GL_KOS_ARRAY_DSTW,
                                                   (pvr_vertex_t *)_glKosVertexBufPointer(),
                                                   count);

            break;

        case GL_TRIANGLE_STRIP:
            if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE1) {
                count = _glKosClipTriangleStripTransformedMT((pvr_vertex_t *)_glKosClipBufAddress(),
                        GL_KOS_ARRAY_DSTW,
                        (pvr_vertex_t *)_glKosVertexBufPointer(),
                        uvsrc,
                        (glTexCoord *)_glKosMultiUVBufPointer(),
                        uvstride,
                        count);
                _glKosMultiUVBufAdd(count);
            }
            else
                count = _glKosClipTriangleStripTransformed((pvr_vertex_t *)_glKosClipBufAddress(),
                        GL_KOS_ARRAY_DSTW,
                        (pvr_vertex_t *)_glKosVertexBufPointer(),
                        count);

            break;

        default:
            count = 0;
            break;
    }

    return count;
}


static inline void _glKosArraysApplyVertexFlags(GLenum mode, pvr_vertex_t *dst, GLuint count) {
    switch(mode) {
        case GL_QUADS:
            _glKosArrayFlagsSetQuad(dst, count);
            break;

        case GL_TRIANGLES:
            _glKosArrayFlagsSetTriangle(dst, count);
            break;

        case GL_TRIANGLE_STRIP:
            _glKosArrayFlagsSetTriangleStrip(dst, count);
            break;
    }
}

static inline void _glKosArraysApplyLighting(pvr_vertex_t *dst, GLuint count) {
    _glKosArraysTransformNormals(GL_KOS_NORMAL_POINTER, count);
    _glKosArraysTransformPositions(GL_KOS_VERTEX_POINTER, count);
    _glKosVertexLights(GL_KOS_ARRAY_BUF, dst, count);
}

static inline void _glKosArraysApplyHeader() {
    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE0) && _glKosBoundTexID() > 0)
        _glKosCompileHdrTx();
    else
        _glKosCompileHdr();
}

static inline pvr_vertex_t *_glKosArraysDest() {
    if(_glKosEnabledNearZClip())
        return _glKosClipBufAddress();

    return _glKosVertexBufPointer();
}

static inline void _glKosArraysFlush(GLuint count) {
    _glKosVertexBufAdd(count);
}

//========================================================================================//
//== OpenGL Elemental Array Submission ==//

//GLAPI void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
//    /* Before we process the vertex data, ensure all parameters are valid */
//    if(!_glKosArraysVerifyParameter(mode, count, type, 1))
//        return;

//    /* Compile the PVR polygon context with the currently enabled flags */
//    _glKosArraysApplyHeader();

//    /* Destination of Output Vertex Array */
//    pvr_vertex_t *dst = _glKosArraysDest();

//    switch(type) {
//        case GL_UNSIGNED_BYTE:
//            GL_KOS_INDEX_POINTER_U8 = (GLubyte *)indices;
//            break;

//        case GL_UNSIGNED_SHORT:
//            GL_KOS_INDEX_POINTER_U16 = (GLushort *)indices;
//            break;
//    }

//    /* Check if Vertex Lighting is enabled. Else, check for Color Submission */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_NORMAL) && _glKosEnabledLighting())
//        _glKosArraysApplyLighting(dst, count);

//    else if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_COLOR) {
//        switch(GL_KOS_COLOR_TYPE) {
//            case GL_FLOAT:
//                switch(GL_KOS_COLOR_COMPONENTS) {
//                    case 3:
//                        switch(type) {
//                            case GL_UNSIGNED_BYTE:
//                                _glKosElementColor3fU8(dst, count);
//                                break;

//                            case GL_UNSIGNED_SHORT:
//                                _glKosElementColor3fU16(dst, count);
//                                break;
//                        }

//                        break;

//                    case 4:
//                        switch(type) {
//                            case GL_UNSIGNED_BYTE:
//                                _glKosElementColor4fU8(dst, count);
//                                break;

//                            case GL_UNSIGNED_SHORT:
//                                _glKosElementColor4fU16(dst, count);
//                                break;
//                        }

//                        break;
//                }

//                break;

//            case GL_UNSIGNED_INT:
//                if(GL_KOS_COLOR_COMPONENTS == 1)
//                    switch(type) {
//                        case GL_UNSIGNED_BYTE:
//                            _glKosElementColor1uiU8(dst, count);
//                            break;

//                        case GL_UNSIGNED_SHORT:
//                            _glKosElementColor1uiU16(dst, count);
//                            break;
//                    }

//                break;

//            case GL_UNSIGNED_BYTE:
//                if(GL_KOS_COLOR_COMPONENTS == 4)
//                    switch(type) {
//                        case GL_UNSIGNED_BYTE:
//                            _glKosElementColor4ubU8(dst, count);
//                            break;

//                        case GL_UNSIGNED_SHORT:
//                            _glKosElementColor4ubU16(dst, count);
//                            break;
//                    }

//                break;
//        }
//    }
//    else
//        _glKosArrayColor0(dst, count); /* No colors bound */

//    /* Check if Texture Coordinates are enabled */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE0) && (_glKosEnabledTexture2D() >= 0))
//        switch(type) {
//            case GL_UNSIGNED_BYTE:
//                _glKosElementTexCoord2fU8(dst, count);
//                break;

//            case GL_UNSIGNED_SHORT:
//                _glKosElementTexCoord2fU16(dst, count);
//                break;
//        }

//    /* Check if Multi Texture Coordinates are enabled */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE1) && (_glKosEnabledTexture2D() >= 0))
//        switch(type) {
//            case GL_UNSIGNED_BYTE:
//                _glKosElementMultiTexCoord2fU8(count);
//                break;

//            case GL_UNSIGNED_SHORT:
//                _glKosElementMultiTexCoord2fU16(count);
//                break;
//        }

//    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

//    if(!(_glKosEnabledNearZClip())) {/* Transform the element vertices */
//        /* Transform vertices with perspective divide */
//        _glKosArraysTransformElements(count);

//        /* Unpack the indexed positions into primitives for rasterization */
//        switch(type) {
//            case GL_UNSIGNED_BYTE:
//                _glKosArraysUnpackElementsS8(dst, count);
//                break;

//            case GL_UNSIGNED_SHORT:
//                _glKosArraysUnpackElementsS16(dst, count);
//                break;
//        }

//        /* Set the vertex flags for use with the PVR */
//        _glKosArraysApplyVertexFlags(mode, dst, count);
//    }
//    else {
//        /* Transform vertices with no perspective divide, store w component */
//        _glKosArraysTransformClipElements(count);

//        /* Unpack the indexed positions into primitives for rasterization */
//        switch(type) {
//            case GL_UNSIGNED_BYTE:
//                _glKosArraysUnpackClipElementsS8(dst, count);
//                break;

//            case GL_UNSIGNED_SHORT:
//                _glKosArraysUnpackClipElementsS16(dst, count);
//                break;
//        }

//        count = _glKosArraysApplyClipping(GL_KOS_ARRAY_BUFUV, 2, mode, count);
//    }

//    _glKosArraysApplyMultiTexture(mode, count);

//    _glKosArraysFlush(count);
//}

//========================================================================================//
//== Array Attribute Functions ==//

//== Color ==//

static inline void _glKosArrayColor1ui(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLbyte *color = (GLbyte *)GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        dst[i].argb = *((GLuint *) color);
        color += GL_KOS_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor4ub(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        element = (GLuint *) color;
        dst[i].argb = RGBA32_2_ARGB32(*element);
        color += GL_KOS_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor3f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLfloat *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        element = (GLfloat *) color;
        dst[i].argb = (0xFF000000 | ((GLubyte)(element[0] * 0xFF)) << 16
                       | ((GLubyte)(element[1] * 0xFF)) << 8
                       | ((GLubyte)(element[2] * 0xFF)));
        color += GL_KOS_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor4f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLfloat *element;
    GLubyte *color = GL_KOS_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        element = (GLfloat *) color;
        dst[i].argb = (((GLubyte)(element[3] * 0xFF)) << 24
                       | ((GLubyte)(element[0] * 0xFF)) << 16
                       | ((GLubyte)(element[1] * 0xFF)) << 8
                       | ((GLubyte)(element[2] * 0xFF)));
        color += GL_KOS_COLOR_STRIDE;
    }
}

//== Texture Coordinates ==//

static inline void _glKosArrayTexCoord2f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLubyte *src = GL_KOS_TEXCOORD0_POINTER;
    GLfloat *uv;

    if(_glKosEnabledTextureMatrix()) {
        _glKosMatrixLoadTexture();

        for(i = 0; i < count; i++) {
            uv = (GLfloat *) src;
            mat_trans_texture2_nomod(uv[0], uv[1], dst[i].u, dst[i].v);

            src += GL_KOS_TEXCOORD0_STRIDE;
        }

        _glKosMatrixLoadRender();
    }
    else {
        for(i = 0; i < count; i++) {
            uv = (GLfloat *) src;
            dst[i].u = uv[0];
            dst[i].v = uv[1];
            src += GL_KOS_TEXCOORD0_STRIDE;
        }
    }
}

static inline void _glKosArrayMultiTexCoord2f(GLuint count) {
    if(_glKosEnabledNearZClip())
        return;

    GLuint i;
    GLubyte *src = GL_KOS_TEXCOORD1_POINTER;
    GLfloat *uv;
    glTexCoord *dst = (glTexCoord *)_glKosMultiUVBufPointer();

    for(i = 0; i < count; i++) {
        uv = (GLfloat *) src;
        dst[i].u = uv[0];
        dst[i].v = uv[1];
        src += GL_KOS_TEXCOORD1_STRIDE;
    }

    _glKosMultiUVBufAdd(count);
}

//========================================================================================//
//== Open GL Draw Arrays ==//

static void _glKosDrawArrays2D(GLenum mode, GLint first, GLsizei count) {
    pvr_vertex_t *dst = _glKosVertexBufPointer();

    /* Check for Color Submission */
    if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_COLOR) {
        switch(GL_KOS_COLOR_TYPE) {
            case GL_FLOAT:
                switch(GL_KOS_COLOR_COMPONENTS) {
                    case 3:
                        _glKosArrayColor3f(dst, count);
                        break;

                    case 4:
                        _glKosArrayColor4f(dst, count);
                        break;
                }

                break;

            case GL_UNSIGNED_INT:
                if(GL_KOS_COLOR_COMPONENTS == 1)
                    _glKosArrayColor1ui(dst, count);

                break;

            case GL_UNSIGNED_BYTE:
                if(GL_KOS_COLOR_COMPONENTS == 4)
                    _glKosArrayColor4ub(dst, count);

                break;
        }
    }
    else
        _glKosArrayColor0(dst, count); /* No colors bound */

    /* Check if Texture Coordinates are enabled */
    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE0) && (_glKosEnabledTexture2D() >= 0))
        _glKosArrayTexCoord2f(dst, count);

    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

    /* Transform Vertex Positions */
    _glKosArraysTransform2D(count);

    /* Set the vertex flags for use with the PVR */
    _glKosArraysApplyVertexFlags(mode, dst, count);

    _glKosArraysFlush(count);
}


//GLAPI void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
//    /* Before we process the vertex data, ensure all parameters are valid */
//    if(!_glKosArraysVerifyParameter(mode, count, first, 0))
//        return;

//    GL_KOS_VERTEX_POINTER   += (first * GL_KOS_VERTEX_STRIDE);       /* Add Pointer Offset */
//    GL_KOS_TEXCOORD0_POINTER += (first * GL_KOS_TEXCOORD0_STRIDE);
//    GL_KOS_TEXCOORD1_POINTER += (first * GL_KOS_TEXCOORD1_STRIDE);
//    GL_KOS_COLOR_POINTER    += (first * GL_KOS_COLOR_STRIDE);
//    GL_KOS_NORMAL_POINTER   += (first * GL_KOS_NORMAL_STRIDE);

//    /* Compile the PVR polygon context with the currently enabled flags */
//    _glKosArraysApplyHeader();

//    if(GL_KOS_VERTEX_SIZE == 2)
//        return _glKosDrawArrays2D(mode, first, count);

//    /* Destination of Output Vertex Array */
//    pvr_vertex_t *dst = _glKosArraysDest();

//    /* Check if Vertex Lighting is enabled. Else, check for Color Submission */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_NORMAL) && _glKosEnabledLighting())
//        _glKosArraysApplyLighting(dst, count);

//    else if(GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_COLOR) {
//        switch(GL_KOS_COLOR_TYPE) {
//            case GL_FLOAT:
//                switch(GL_KOS_COLOR_COMPONENTS) {
//                    case 3:
//                        _glKosArrayColor3f(dst, count);
//                        break;

//                    case 4:
//                        _glKosArrayColor4f(dst, count);
//                        break;
//                }

//                break;

//            case GL_UNSIGNED_INT:
//                if(GL_KOS_COLOR_COMPONENTS == 1)
//                    _glKosArrayColor1ui(dst, count);

//                break;

//            case GL_UNSIGNED_BYTE:
//                if(GL_KOS_COLOR_COMPONENTS == 4)
//                    _glKosArrayColor4ub(dst, count);

//                break;
//        }
//    }
//    else
//        _glKosArrayColor0(dst, count); /* No colors bound, color white */

//    /* Check if Texture Coordinates are enabled */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE0) && (_glKosEnabledTexture2D() >= 0))
//        _glKosArrayTexCoord2f(dst, count);

//    /* Check if Multi Texture Coordinates are enabled */
//    if((GL_KOS_VERTEX_PTR_MODE & GL_KOS_USE_TEXTURE1) && (_glKosEnabledTexture2D() >= 0))
//        _glKosArrayMultiTexCoord2f(count);

//    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

//    if(!_glKosEnabledNearZClip()) { /* No NearZ Clipping Enabled */
//        /* Transform Vertex Positions */
//        _glKosArraysTransform(count);

//        /* Set the vertex flags for use with the PVR */
//        _glKosArraysApplyVertexFlags(mode, dst, count);
//    }
//    else { /* NearZ Clipping is Enabled */
//        /* Transform vertices with no perspective divide, store w component */
//        _glKosArraysTransformClip(count);

//        /* Finally, clip the input vertex data into the output vertex buffer */
//        count = _glKosArraysApplyClipping(
//            (GLfloat *) GL_KOS_TEXCOORD1_POINTER,
//            GL_KOS_TEXCOORD1_STRIDE / 4,
//            mode, count
//        );
//        /* FIXME: The above function should really accept a GLbyte* and the stride directly
//           but that affects a lot of other code, so dividing the stride by float size
//           hopefully results in the same thing for now */
//    }

//    _glKosArraysApplyMultiTexture(mode, count);

//    _glKosArraysFlush(count);
//}


