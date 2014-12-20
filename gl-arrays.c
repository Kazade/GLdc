/* KallistiGL for KallistiOS ##version##

   libgl/gl-arrays.c
   Copyright (C) 2013-2014 Josh Pearson

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
#include "gl-api.h"
#include "gl-arrays.h"
#include "gl-rgb.h"
#include "gl-sh4.h"

//========================================================================================//
//== Local Variables ==//

#define GL_MAX_ARRAY_VERTICES 1024*32 /* Maximum Number of Vertices in the Array Buffer */
static glVertex  GL_ARRAY_BUF[GL_MAX_ARRAY_VERTICES];
static GLfloat   GL_ARRAY_BUFW[GL_MAX_ARRAY_VERTICES];
static GLfloat   GL_ARRAY_DSTW[GL_MAX_ARRAY_VERTICES];
static glVertex *GL_ARRAY_BUF_PTR;
static GLuint GL_VERTEX_PTR_MODE = 0;

//========================================================================================//
//== Local Function Definitions ==//

static inline void _glKosArraysTransformNormals(GLfloat *normal, GLuint count);
static inline void _glKosArraysTransformPositions(GLfloat *position, GLuint count);

//========================================================================================//
//== Open GL API Public Functions ==//

/* Submit a Vertex Position Pointer */
GLAPI void APIENTRY glVertexPointer(GLint size, GLenum type,
                                    GLsizei stride, const GLvoid *pointer) {
    if(size != 3) /* Expect 3D X,Y,Z vertex... could do 2d X,Y later */
        _glKosThrowError(GL_INVALID_VALUE, "glVertexPointer");

    if(type != GL_FLOAT) /* Expect Floating point vertices */
        _glKosThrowError(GL_INVALID_ENUM, "glVertexPointer");

    if(stride < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glVertexPointer");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    (stride) ? (GL_VERTEX_STRIDE = stride / 4) : (GL_VERTEX_STRIDE = 3);

    GL_VERTEX_POINTER = (float *)pointer;

    GL_VERTEX_PTR_MODE |= GL_USE_ARRAY;
}

/* Submit a Vertex Normal Pointer */
GLAPI void APIENTRY glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer) {
    if(type != GL_FLOAT) /* Expect Floating point vertices */
        _glKosThrowError(GL_INVALID_ENUM, "glNormalPointer");

    if(stride < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glNormalPointer");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    (stride) ? (GL_NORMAL_STRIDE = stride / 4) : (GL_NORMAL_STRIDE = 3);

    GL_NORMAL_POINTER = (float *)pointer;

    GL_VERTEX_PTR_MODE |= GL_USE_NORMAL;
}

/* Submit a Texture Coordinate Pointer */
GLAPI void APIENTRY glTexCoordPointer(GLint size, GLenum type,
                                      GLsizei stride, const GLvoid *pointer) {
    if(size != 2)  /* Expect u and v */
        _glKosThrowError(GL_INVALID_VALUE, "glTexCoordPointer");

    if(type != GL_FLOAT) /* Expect Floating point vertices */
        _glKosThrowError(GL_INVALID_ENUM, "glTexCoordPointer");

    if(stride < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glTexCoordPointer");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    (stride) ? (GL_TEXCOORD_STRIDE = stride / 4) : (GL_TEXCOORD_STRIDE = 2);

    GL_TEXCOORD_POINTER = (float *)pointer;

    GL_VERTEX_PTR_MODE |= GL_USE_TEXTURE;
}

/* Submit a Color Pointer */
GLAPI void APIENTRY glColorPointer(GLint size, GLenum type,
                                   GLsizei stride, const GLvoid *pointer) {
    if((type == GL_UNSIGNED_INT) && (size == 1)) {
        GL_COLOR_COMPONENTS = 1;
        GL_COLOR_POINTER = (GLvoid *)pointer;
        GL_COLOR_TYPE = type;
    }
    else if((type == GL_UNSIGNED_BYTE) && (size == 4)) {
        GL_COLOR_COMPONENTS = 4;
        GL_COLOR_POINTER = (GLvoid *)pointer;
        GL_COLOR_TYPE = type;
    }
    else if((type == GL_FLOAT) && (size == 3)) {
        GL_COLOR_COMPONENTS = 3;
        GL_COLOR_POINTER = (GLfloat *)pointer;
        GL_COLOR_TYPE = type;
    }
    else if((type == GL_FLOAT) && (size == 4)) {
        GL_COLOR_COMPONENTS = 4;
        GL_COLOR_POINTER = (GLfloat *)pointer;
        GL_COLOR_TYPE = type;
    }
    else {
        _glKosThrowError(GL_INVALID_ENUM, "glColorPointer");
        _glKosPrintError();
        return;
    }

    (stride) ? (GL_COLOR_STRIDE = stride / 4) : (GL_COLOR_STRIDE = size);

    GL_VERTEX_PTR_MODE |= GL_USE_COLOR;
}
//========================================================================================//
//== Vertex Pointer Internal API ==//

inline void _glKosArrayBufIncrement() {
    ++GL_ARRAY_BUF_PTR;
}

inline void _glKosArrayBufReset() {
    GL_ARRAY_BUF_PTR = &GL_ARRAY_BUF[0];
}

inline glVertex *_glKosArrayBufAddr() {
    return &GL_ARRAY_BUF[0];
}

inline glVertex *_glKosArrayBufPtr() {
    return GL_ARRAY_BUF_PTR;
}

static inline void _glKosArraysTransformNormals(GLfloat *normal, GLuint count) {
    glVertex *v = &GL_ARRAY_BUF[0];
    GLfloat *N = normal;

    _glKosMatrixLoadModelRot();

    while(count--) {
        mat_trans_normal3_nomod(N[0], N[1], N[2], v->norm[0], v->norm[1], v->norm[2]);
        N += 3;
        ++v;
    }
}

static inline void _glKosArraysTransformPositions(GLfloat *position, GLuint count) {
    glVertex *v = &GL_ARRAY_BUF[0];
    GLfloat *P = position;

    _glKosMatrixLoadModelView();

    while(count--) {
        mat_trans_single3_nodiv_nomod(P[0], P[1], P[2], v->pos[0], v->pos[1], v->pos[2]);
        P += 3;
        ++v;
    }
}

//========================================================================================//
//== Arrays Vertex Transform ==/

static void _glKosArraysTransform(GLuint count) {
    GLfloat *src = GL_VERTEX_POINTER;
    pvr_vertex_t *dst = _glKosVertexBufPointer();

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");
    
    while(count--)  {
        __x = src[0];
        __y = src[1];
        __z = src[2];

        mat_trans_fv12()

        dst->x = __x;
        dst->y = __y;
        dst->z = __z;

        ++dst;
        
        src += GL_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransformClip(GLuint count) {
    GLfloat *src = GL_VERTEX_POINTER;
    GLfloat *W = GL_ARRAY_DSTW;
    pvr_vertex_t *dst = _glKosClipBufAddress();

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");
    register float __w  __asm__("fr15");

    while(count--)  {
        __x = src[0];
        __y = src[1];
        __z = src[2];

        mat_trans_fv12_nodivw()

        dst->x = __x;
        dst->y = __y;
        dst->z = __z;
        *W++ = __w;
        
        ++dst;
        
        src += GL_VERTEX_STRIDE;        
    }
}

static void _glKosArraysTransformElements(GLuint count) {
    GLfloat *src = GL_VERTEX_POINTER;
    GLuint i = 0;

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    printf("Transform Elements()\n");

    for(i = 0; i < count; i++) {
        __x = src[0];
        __y = src[1];
        __z = src[2];

        mat_trans_fv12()

        GL_ARRAY_BUF[i].pos[0] = __x;
        GL_ARRAY_BUF[i].pos[1] = __y;
        GL_ARRAY_BUF[i].pos[2] = __z;

        src += GL_VERTEX_STRIDE;
    }
}

static void _glKosArraysTransformClipElements(GLuint count) {
    GLfloat *src = GL_VERTEX_POINTER;
    GLuint i;

    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");
    register float __w  __asm__("fr15");

    for(i = 0; i < count; i++) {
        __x = src[0];
        __y = src[1];
        __z = src[2];

        mat_trans_fv12_nodivw()

        GL_ARRAY_BUF[i].pos[0] = __x;
        GL_ARRAY_BUF[i].pos[1] = __y;
        GL_ARRAY_BUF[i].pos[2] = __z;
        GL_ARRAY_BUFW[i] = __w;

        src += GL_VERTEX_STRIDE;
    }
}

//========================================================================================//
//== Element Attribute Functions ==//

//== Color ==//

static inline void _glKosElementColor0(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i++)
        dst[i].argb = _glKosVertexColor();
}

static inline void _glKosElementColor1uiU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *src = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)
        dst[i].argb = src[GL_INDEX_POINTER_U8[i] * GL_COLOR_STRIDE];
}

static inline void _glKosElementColor1uiU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *color = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)
        dst[i].argb = color[GL_INDEX_POINTER_U16[i] * GL_COLOR_STRIDE];
}

static inline void _glKosElementColor4ubU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, *color = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)
        dst[i].argb = RGBA32_2_ARGB32(color[GL_INDEX_POINTER_U8[i]] * GL_COLOR_STRIDE);
}

static inline void _glKosElementColor4ubU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, *color = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)
        dst[i].argb = RGBA32_2_ARGB32(color[GL_INDEX_POINTER_U16[i] * GL_COLOR_STRIDE]);
}

static inline void _glKosElementColor3fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgb3f *color = (GLrgb3f *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index =  GL_INDEX_POINTER_U8[i] * GL_COLOR_STRIDE;
        dst[i].argb = (0xFF000000 | ((GLubyte)color[index][0] * 0xFF) << 16
                       | ((GLubyte)color[index][1] * 0xFF) << 8
                       | ((GLubyte)color[index][2] * 0xFF));
    }
}

static inline void _glKosElementColor3fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgb3f *color = (GLrgb3f *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index =  GL_INDEX_POINTER_U16[i] * GL_COLOR_STRIDE;
        dst[i].argb = (0xFF000000 | ((GLubyte)color[index][0] * 0xFF) << 16
                       | ((GLubyte)color[index][1] * 0xFF) << 8
                       | ((GLubyte)color[index][2] * 0xFF));
    }
}

static inline void _glKosElementColor4fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgba4f *color = (GLrgba4f *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_INDEX_POINTER_U8[i] * GL_COLOR_STRIDE;
        dst[i].argb = (((GLubyte)color[index][3] * 0xFF) << 24
                       | ((GLubyte)color[index][0] * 0xFF) << 16
                       | ((GLubyte)color[index][1] * 0xFF) << 8
                       | ((GLubyte)color[index][2] * 0xFF));
    }
}

static inline void _glKosElementColor4fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLrgba4f *color = (GLrgba4f *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_INDEX_POINTER_U16[i] * GL_COLOR_STRIDE;
        dst[i].argb = (((GLubyte)color[index][3] * 0xFF) << 24
                       | ((GLubyte)color[index][0] * 0xFF) << 16
                       | ((GLubyte)color[index][1] * 0xFF) << 8
                       | ((GLubyte)color[index][2] * 0xFF));
    }
}

//== Texture Coordinates ==//

static inline void _glKosElementTexCoord2fU16(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLfloat *t = GL_TEXCOORD_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_INDEX_POINTER_U16[i] * GL_TEXCOORD_STRIDE;
        dst[i].u = t[index];
        dst[i].v = t[index + 1];
    }
}

static inline void _glKosElementTexCoord2fU8(pvr_vertex_t *dst, GLuint count) {
    GLuint i, index;
    GLfloat *t = GL_TEXCOORD_POINTER;

    for(i = 0; i < count; i++) {
        index = GL_INDEX_POINTER_U8[i] * GL_TEXCOORD_STRIDE;
        dst[i].u = t[index];
        dst[i].v = t[index + 1];
    }
}

//========================================================================================//
//== Element Unpacking ==//

static inline void _glKosArraysUnpackElementsS16(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_INDEX_POINTER_U16[i]].pos[0];
        dst[i].y = vert[GL_INDEX_POINTER_U16[i]].pos[1];
        dst[i].z = vert[GL_INDEX_POINTER_U16[i]].pos[2];
    }
}

static inline void _glKosArraysUnpackElementsS8(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_INDEX_POINTER_U8[i]].pos[0];
        dst[i].y = vert[GL_INDEX_POINTER_U8[i]].pos[1];
        dst[i].z = vert[GL_INDEX_POINTER_U8[i]].pos[2];
    }
}

static inline void _glKosArraysUnpackClipElementsS16(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_INDEX_POINTER_U16[i]].pos[0];
        dst[i].y = vert[GL_INDEX_POINTER_U16[i]].pos[1];
        dst[i].z = vert[GL_INDEX_POINTER_U16[i]].pos[2];
        GL_ARRAY_DSTW[i] = GL_ARRAY_BUFW[GL_INDEX_POINTER_U16[i]];
    }
}

static inline void _glKosArraysUnpackClipElementsS8(pvr_vertex_t *dst, GLuint count) {
    glVertex *vert = GL_ARRAY_BUF;
    GLuint i;

    for(i = 0; i < count; i++) {
        dst[i].x = vert[GL_INDEX_POINTER_U8[i]].pos[0];
        dst[i].y = vert[GL_INDEX_POINTER_U8[i]].pos[1];
        dst[i].z = vert[GL_INDEX_POINTER_U8[i]].pos[2];
        GL_ARRAY_DSTW[i] = GL_ARRAY_BUFW[GL_INDEX_POINTER_U8[i]];
    }
}

//========================================================================================//
//== Misc Utils ==//

static inline void _glKosVertexSwizzle(pvr_vertex_t *v1, pvr_vertex_t *v2) {
    pvr_vertex_t tmp = *v1;
    *v1 = *v2;
    *v2 = * &tmp;
}

static inline void _glKosArraysResetState() {
    GL_VERTEX_PTR_MODE = 0;
}

//========================================================================================//
//== Vertex Flag Settings for the PVR2DC hardware ==//

static inline void _glKosArrayFlagsSetQuad(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i += 4) {
        _glKosVertexSwizzle(&dst[2], &dst[3]);
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

//========================================================================================//
//== OpenGL Error Code Genration ==//

static GLuint _glKosArraysVerifyParameter(GLenum mode, GLsizei count, GLenum type, GLubyte element) {
    GLuint GL_ERROR_CODE = 0;

    if(mode != GL_QUADS)
        if(mode != GL_TRIANGLES)
            if(mode != GL_TRIANGLE_STRIP)
                GL_ERROR_CODE |= GL_INVALID_ENUM;

    if(count < 0)
        GL_ERROR_CODE |= GL_INVALID_VALUE;

    if(!(GL_VERTEX_PTR_MODE & GL_USE_ARRAY))
        GL_ERROR_CODE |= GL_INVALID_OPERATION;

    if(count > GL_MAX_ARRAY_VERTICES)
        GL_ERROR_CODE |= GL_OUT_OF_MEMORY;

    if(element) {
        switch(type) {
            case GL_UNSIGNED_BYTE:
            case GL_UNSIGNED_SHORT:
                break;

            default:
                GL_ERROR_CODE |= GL_INVALID_ENUM;
        }
    }
    else if(type > count)
        GL_ERROR_CODE |= GL_INVALID_VALUE;

    return GL_ERROR_CODE;
}

void _glKosPrintErrorString(GLuint error) {
    if(error) {
        printf("GL_ERROR_CODE GENERATED:\n");

        if(error & GL_INVALID_ENUM)
            printf("\tGL_INVALID_ENUM\n");

        if(error & GL_INVALID_VALUE)
            printf("\tGL_INVALID_VALUE\n");

        if(error & GL_INVALID_OPERATION)
            printf("\tGL_INVALID__OPERATION\n");

        if(error & GL_OUT_OF_MEMORY)
            printf("\tGL_OUT_OF_MEMORY\n");
    }
}

//========================================================================================//
//== OpenGL Elemental Array Submission ==//

GLAPI void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
    /* Before we process the vertex data, ensure all parameters are valid */
    GLuint error = _glKosArraysVerifyParameter(mode, count, type, 1);

    if(error) {
        _glKosPrintErrorString(error);

        _glKosArraysResetState();

        return;
    }

    switch(type) {
        case GL_UNSIGNED_BYTE:
            GL_INDEX_POINTER_U8 = (GLubyte *)indices;
            break;

        case GL_UNSIGNED_SHORT:
            GL_INDEX_POINTER_U16 = (GLushort *)indices;
            break;
    }

    /* Compile the PVR polygon context with the currently enabled flags */
    if((GL_VERTEX_PTR_MODE & GL_USE_TEXTURE) && _glKosBoundTexID() > 0)
        _glKosCompileHdrTx();
    else
        _glKosCompileHdr();

    pvr_vertex_t *dst; /* Destination of Output Vertex Array */

    if(_glKosEnabledNearZClip())
        dst = _glKosClipBufAddress();
    else
        dst = _glKosVertexBufPointer();

    /* Check if Vertex Lighting is enabled. Else, check for Color Submission */
    if((GL_VERTEX_PTR_MODE & GL_USE_NORMAL) && _glKosEnabledLighting()) {
        _glKosArraysTransformNormals(GL_NORMAL_POINTER, count);
        _glKosArraysTransformPositions(GL_VERTEX_POINTER, count);
        _glKosVertexLights(GL_ARRAY_BUF, dst, count);
    }
    else if(GL_VERTEX_PTR_MODE & GL_USE_COLOR) {
        switch(GL_COLOR_TYPE) {
            case GL_FLOAT:
                switch(GL_COLOR_COMPONENTS) {
                    case 3:
                        switch(type) {
                            case GL_UNSIGNED_BYTE:
                                _glKosElementColor3fU8(dst, count);
                                break;

                            case GL_UNSIGNED_SHORT:
                                _glKosElementColor3fU16(dst, count);
                                break;
                        }

                        break;

                    case 4:
                        switch(type) {
                            case GL_UNSIGNED_BYTE:
                                _glKosElementColor4fU8(dst, count);
                                break;

                            case GL_UNSIGNED_SHORT:
                                _glKosElementColor4fU16(dst, count);
                                break;
                        }

                        break;
                }

                break;

            case GL_UNSIGNED_INT:
                if(GL_COLOR_COMPONENTS == 1)
                    switch(type) {
                        case GL_UNSIGNED_BYTE:
                            _glKosElementColor1uiU8(dst, count);
                            break;

                        case GL_UNSIGNED_SHORT:
                            _glKosElementColor1uiU16(dst, count);
                            break;
                    }

                break;

            case GL_UNSIGNED_BYTE:
                if(GL_COLOR_COMPONENTS == 4)
                    switch(type) {
                        case GL_UNSIGNED_BYTE:
                            _glKosElementColor4ubU8(dst, count);
                            break;

                        case GL_UNSIGNED_SHORT:
                            _glKosElementColor4ubU16(dst, count);
                            break;
                    }

                break;
        }
    }
    else
        _glKosElementColor0(dst, count); /* No colors bound, color white */

    /* Check if Texture Coordinates are enabled */
    if((GL_VERTEX_PTR_MODE & GL_USE_TEXTURE) && (_glKosEnabledTexture2D() >= 0))
        switch(type) {
            case GL_UNSIGNED_BYTE:
                _glKosElementTexCoord2fU8(dst, count);
                break;

            case GL_UNSIGNED_SHORT:
                _glKosElementTexCoord2fU16(dst, count);
                break;
        }

    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

    if(!(_glKosEnabledNearZClip())) {/* Transform the element vertices */
        /* Transform vertices with perspective divde */
        _glKosArraysTransformElements(count);

        /* Unpack the indexed positions into primitives for rasterization */
        switch(type) {
            case GL_UNSIGNED_BYTE:
                _glKosArraysUnpackElementsS8(dst, count);
                break;

            case GL_UNSIGNED_SHORT:
                _glKosArraysUnpackElementsS16(dst, count);
                break;
        }

        /* Set the vertex flags for use with the PVR */
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
    else {
        /* Transform vertices with no perspective divde, store w component */
        _glKosArraysTransformClipElements(count);

        /* Unpack the indexed positions into primitives for rasterization */
        switch(type) {
            case GL_UNSIGNED_BYTE:
                _glKosArraysUnpackClipElementsS8(dst, count);
                break;

            case GL_UNSIGNED_SHORT:
                _glKosArraysUnpackClipElementsS16(dst, count);
                break;
        }

        /* Finally, clip the input vertex data into the output vertex buffer */
        switch(mode) {
            case GL_TRIANGLES:
                count = _glKosClipTrianglesTransformed(dst, GL_ARRAY_DSTW,
                                                       (pvr_vertex_t *)_glKosVertexBufPointer(), count);
                break;

            case GL_QUADS:
                count = _glKosClipQuadsTransformed(dst, GL_ARRAY_DSTW,
                                                   (pvr_vertex_t *)_glKosVertexBufPointer(), count);

                break;

            case GL_TRIANGLE_STRIP:
                count = _glKosClipTriangleStripTransformed(dst, GL_ARRAY_DSTW,
                        (pvr_vertex_t *)_glKosVertexBufPointer(), count);
                break;
        }
    }

    _glKosVertexBufAdd(count);

    _glKosArraysResetState();
}

//========================================================================================//
//== Array Attribute Functions ==//

//== Color ==//

static inline void _glKosArrayColor0(pvr_vertex_t *dst, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i++)
        dst[i].argb = _glKosVertexColor();
}

static inline void _glKosArrayColor1ui(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLuint *color = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++) {
        dst[i].argb = *color;
        color += GL_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor4ub(pvr_vertex_t *dst, GLuint count) {
    GLuint i, *color = (GLuint *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        dst[i].argb = RGBA32_2_ARGB32(*color);
        color += GL_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor3f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLfloat *color = (GLfloat *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        dst[i].argb = (0xFF000000 | ((GLubyte)(color[0] * 0xFF)) << 16
                       | ((GLubyte)(color[1] * 0xFF)) << 8
                       | ((GLubyte)(color[2] * 0xFF)));
        color += GL_COLOR_STRIDE;
    }
}

static inline void _glKosArrayColor4f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLfloat *color = (GLfloat *)GL_COLOR_POINTER;

    for(i = 0; i < count; i++)  {
        dst[i].argb = (((GLubyte)(color[3] * 0xFF)) << 24
                       | ((GLubyte)(color[0] * 0xFF)) << 16
                       | ((GLubyte)(color[1] * 0xFF)) << 8
                       | ((GLubyte)(color[2] * 0xFF)));
        color += GL_COLOR_STRIDE;
    }
}

//== Texture Coordinates ==//

static inline void _glKosArrayTexCoord2f(pvr_vertex_t *dst, GLuint count) {
    GLuint i;
    GLfloat *uv = GL_TEXCOORD_POINTER;

    for(i = 0; i < count; i++) {
        dst[i].u = uv[0];
        dst[i].v = uv[1];
        uv += GL_TEXCOORD_STRIDE;
    }
}

//========================================================================================//
//== Open GL Draw Arrays ==//

GLAPI void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    /* Before we process the vertex data, ensure all parameters are valid */
    GLuint error = _glKosArraysVerifyParameter(mode, count, first, 0);

    if(error) {
        _glKosPrintErrorString(error);

        return _glKosArraysResetState();
    }

    GL_VERTEX_POINTER   += first;       /* Add Pointer Offset */
    GL_TEXCOORD_POINTER += first;
    GL_COLOR_POINTER    += first;
    GL_NORMAL_POINTER   += first;

    /* Compile the PVR polygon context with the currently enabled flags */
    if((GL_VERTEX_PTR_MODE & GL_USE_TEXTURE) && _glKosBoundTexID() > 0)
        _glKosCompileHdrTx();
    else
        _glKosCompileHdr();

    pvr_vertex_t *dst; /* Destination of Output Vertex Array */

    if(_glKosEnabledNearZClip())
        dst = _glKosClipBufAddress();
    else
        dst = _glKosVertexBufPointer();

    /* Check if Vertex Lighting is enabled. Else, check for Color Submission */
    if((GL_VERTEX_PTR_MODE & GL_USE_NORMAL) && _glKosEnabledLighting()) {
        _glKosArraysTransformNormals(GL_NORMAL_POINTER, count);
        _glKosArraysTransformPositions(GL_VERTEX_POINTER, count);
        _glKosVertexLights(GL_ARRAY_BUF, dst, count);
    }
    else if(GL_VERTEX_PTR_MODE & GL_USE_COLOR) {
        switch(GL_COLOR_TYPE) {
            case GL_FLOAT:
                switch(GL_COLOR_COMPONENTS) {
                    case 3:
                        _glKosArrayColor3f(dst, count);
                        break;

                    case 4:
                        _glKosArrayColor4f(dst, count);
                        break;
                }

                break;

            case GL_UNSIGNED_INT:
                if(GL_COLOR_COMPONENTS == 1)
                    _glKosArrayColor1ui(dst, count);

                break;

            case GL_UNSIGNED_BYTE:
                if(GL_COLOR_COMPONENTS == 4)
                    _glKosArrayColor4ub(dst, count);

                break;
        }
    }
    else
        _glKosArrayColor0(dst, count); /* No colors bound, color white */

    /* Check if Texture Coordinates are enabled */
    if((GL_VERTEX_PTR_MODE & GL_USE_TEXTURE) && (_glKosEnabledTexture2D() >= 0))
        _glKosArrayTexCoord2f(dst, count);

    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

    if(!_glKosEnabledNearZClip()) {
        /* Transform Vertex Positions */
        _glKosArraysTransform(count);

        /* Set the vertex flags for use with the PVR */
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
    else {
        /* Transform vertices with no perspective divde, store w component */
        _glKosArraysTransformClip(count);

        /* Finally, clip the input vertex data into the output vertex buffer */
        switch(mode) {
            case GL_TRIANGLES:
                count = _glKosClipTrianglesTransformed(dst, GL_ARRAY_DSTW,
                                                       (pvr_vertex_t *)_glKosVertexBufPointer(), count);
                break;

            case GL_QUADS:
                count = _glKosClipQuadsTransformed(dst, GL_ARRAY_DSTW,
                                                   (pvr_vertex_t *)_glKosVertexBufPointer(), count);

                break;

            case GL_TRIANGLE_STRIP:
                count = _glKosClipTriangleStripTransformed(dst, GL_ARRAY_DSTW,
                        (pvr_vertex_t *)_glKosVertexBufPointer(), count);
                break;
        }
    }

    _glKosVertexBufAdd(count);

    _glKosArraysResetState();
}