/* KallistiGL for KallistiOS ##version##

   libgl/gl-api.h
   Copyright (C) 2013-2014 Josh Pearson

   The functions defined in this header are for internal use by the API,
   and not for use externally.
*/

#ifndef GL_API_H
#define GL_API_H

#include "gl.h"

typedef struct {
    GLfloat pos[3];
    GLfloat norm[3];
} glVertex; /* Simple Vertex used for Dynamic Vertex Lighting */

typedef struct {
    GLfloat u, v;
} glTexCoord; /* Simple Texture Coordinate used for Multi-Texturing */

typedef struct {
    GLfloat s, t, r, q;
} glTexCoord4f; /* 3D Texture Coordinate */

typedef struct {
    GLushort width;
    GLushort height;
    GLuint   color; /* This is the PVR texture format */
    GLubyte  env;
    GLubyte  filter;
    GLubyte  mip_map;
    GLubyte  uv_clamp;
    GLuint   index;
    GLvoid *data;
} GL_TEXTURE_OBJECT; /* KOS Open GL Texture Object */

typedef struct {
    GLuint  texID;
    GLsizei index;
    GLvoid *data;
    GLvoid *link;
} GL_FRAMEBUFFER_OBJECT; /* KOS Open GL Frame Buffer Object */

typedef struct {
    pvr_poly_hdr_t hdr;
    pvr_vertex_t *src;
    GLuint count;
} GL_MULTITEX_OBJECT;

typedef GLushort uint16;
typedef GLubyte  uint8;

/* Vertex Main Buffer Internal Functions */
void  _glKosVertexBufSwitchOP();
void  _glKosVertexBufSwitchTR();
void *_glKosVertexBufAddress(unsigned char list);
void *_glKosVertexBufPointer();
void *_glKosTRVertexBufPointer();
void  _glKosVertexBufIncrement();
void  _glKosTRVertexBufIncrement();
void  _glKosVertexBufAdd(unsigned int count);
void  _glKosTRVertexBufAdd(unsigned int count);
void  _glKosVertexBufDecrement();
void  _glKosVertexBufReset();
unsigned int _glKosVertexBufCount(unsigned char list);
unsigned char _glKosList();
void _glKosVertexBufCopy(void *src, void *dst, GLuint count);
void _glKosResetEnabledTex();
void *_glKosMultiUVBufAddress();
void *_glKosMultiUVBufPointer();
void _glKosMultiUVBufIncrement();
void _glKosMultiUVBufAdd(GLuint count);
void _glKosMultiUVBufReset();

/* Vertex Clip Buffer Internal Functions */
void *_glKosClipBufAddress();
void *_glKosClipBufPointer();
void  _glKosClipBufIncrement();
void  _glKosClipBufReset();

/* Vertex Array Buffer Internal Functions */
void      _glKosArrayBufIncrement();
void      _glKosArrayBufReset();
glVertex *_glKosArrayBufAddr();
glVertex *_glKosArrayBufPtr();

/* Initialize the OpenGL PVR Pipeline */
int  _glKosInitPVR();
unsigned char  _glKosInitTextures();

/* Compile the current Polygon Header for the PVR */
void _glKosCompileHdr();
void _glKosCompileHdrTx();
void _glKosCompileHdrMTx();
void _glKosCompileHdrT(GL_TEXTURE_OBJECT *tex);
void _glKosCompileHdrMT(pvr_poly_hdr_t *dst, GL_TEXTURE_OBJECT *tex);

/* Clipping Internal Functions */
void         _glKosTransformClipBuf(pvr_vertex_t *v, GLuint verts);
unsigned int _glKosClipTriangleStrip(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipTriangles(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipQuads(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);

unsigned int _glKosClipTrianglesTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count);
unsigned int _glKosClipQuadsTransformed(pvr_vertex_t *vin, float *w, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipTriangleStripTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count);

unsigned int _glKosClipTrianglesTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride, GLuint count);
unsigned int _glKosClipTriangleStripTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride, GLuint count);
unsigned int _glKosClipQuadsTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride, GLuint count);


/* Lighting Internal Functions */
void _glKosInitLighting();
void _glKosEnableLight(const GLuint light);
void _glKosDisableLight(const GLuint light);
void _glKosSetEyePosition(GLfloat *position);
void _glKosVertexComputeLighting(pvr_vertex_t *v, int verts);
void _glKosVertexLight(glVertex *P, pvr_vertex_t *v);
unsigned int _glKosVertexLightColor(glVertex *P);
void _glKosVertexLights(glVertex *P, pvr_vertex_t *v, GLuint count);

/* Vertex Position Submission Internal Functions */
void _glKosVertex3ft(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3ftv(const GLfloat *xyz);
void _glKosVertex3fc(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fcv(const GLfloat *xyz);
void _glKosVertex3fp(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fpv(const GLfloat *xyz);
void _glKosVertex3fl(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3flv(const GLfloat *xyz);
void _glKosVertex3flc(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3flcv(const GLfloat *xyz);
void _glKosVertex3fs(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fsv(const GLfloat *xyz);

/* Matrix Internal Functions */
void _glKosInitMatrix();
void _glKosMatrixLoadModelView();
void _glKosMatrixLoadModelRot();
void _glKosMatrixApplyScreenSpace();
void _glKosMatrixApplyRender();
void _glKosMatrixLoadRender();
void _glKosMatrixLoadTexture();

/* API Enabled Capabilities Internal Functions */
GLubyte _glKosEnabledBlend();
GLubyte _glKosEnabledTexture2D();
GLubyte _glKosEnabledNearZClip();
GLubyte _glKosEnabledLighting();
GLubyte _glKosEnabledFog();
GLubyte _glKosEnabledCulling();
GLubyte _glKosEnabledScissorTest();
GLubyte _glKosEnabledDepthTest();

/* RGB Pixel Colorspace Internal Functions */
uint16 __glKosAverageQuadPixelRGB565(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageQuadPixelARGB1555(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageQuadPixelARGB4444(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageBiPixelRGB565(uint16 p1, uint16 p2);
uint16 __glKosAverageBiPixelARGB1555(uint16 p1, uint16 p2);
uint16 __glKosAverageBiPixelARGB4444(uint16 p1, uint16 p2);

/* Render-To-Texture Functions */
void _glKosInitFrameBuffers();

/* Error Codes */
void _glKosThrowError(GLenum error, char *functionName);
void _glKosPrintError();
GLubyte _glKosHasError();

GLuint  _glKosTextureWidth(GLuint index);
GLuint  _glKosTextureHeight(GLuint index);
GLvoid *_glKosTextureData(GLuint index);

/* Frame Buffer Object Internal Functions */
GLsizei _glKosGetFBO();
GLuint  _glKosGetFBOWidth(GLsizei fbi);
GLuint  _glKosGetFBOHeight(GLsizei fbi);
GLvoid *_glKosGetFBOData(GLsizei fbi);

/* Internal State Cap Accessors */
GLubyte _glKosEnabledDepthTest();
GLubyte _glKosEnabledScissorTest();
GLubyte _glKosEnabledCulling();
GLubyte _glKosEnabledFog();
GLubyte _glKosEnabledLighting();
GLubyte _glKosEnabledNearZClip();
GLubyte _glKosEnabledTexture2D();
GLubyte _glKosEnabledBlend();
GLuint  _glKosBlendSrcFunc();
GLuint  _glKosBlendDstFunc();
GLubyte _glKosCullFaceMode();
GLubyte _glKosCullFaceFront();
GLuint  _glKosDepthFunc();
GLubyte _glKosDepthMask();
GLubyte _glKosIsLightEnabled(GLubyte light);
GLubyte _glKosGetMaxLights();
GLuint  _glKosBoundTexID();
GLuint  _glKosVertexColor();
GLubyte _glKosMaxTextureUnits();
GLubyte _glKosEnabledTextureMatrix();

GL_TEXTURE_OBJECT *_glKosBoundMultiTexObject();
GLuint _glKosActiveTextureBoundTexID();

void _glKosPushMultiTexObject(GL_TEXTURE_OBJECT *tex,
                                     pvr_vertex_t *src,
                                     GLuint count);

static inline void _glKosVertexCopyPVR(const pvr_vertex_t *src, pvr_vertex_t *dst) {
    *dst = *src;
}

static inline void _glKosTexCoordCopy(const glTexCoord *src, glTexCoord *dst) {
    *dst = *src;
}

#endif
