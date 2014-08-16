/* KallistiGL for KallistiOS ##version##

   libgl/gl-api.h
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   The functions defined in this header are for internal use by the API,
   and not for use externally.
*/

#ifndef GL_API_H
#define GL_API_H

typedef struct {
    float pos[3];
    float norm[3];
} glVertex; /* Simple Vertex used for Dynamic Vertex Lighting */

typedef unsigned short uint16;
typedef unsigned char  uint8;

/* Vertex Main Buffer Internal Functions */
inline void  _glKosVertexBufSwitchOP();
inline void  _glKosVertexBufSwitchTR();
inline void *_glKosVertexBufAddress(unsigned char list);
inline void *_glKosVertexBufPointer();
inline void *_glKosTRVertexBufPointer();
inline void  _glKosVertexBufIncrement();
inline void  _glKosTRVertexBufIncrement();
inline void  _glKosVertexBufAdd(unsigned int count);
inline void  _glKosTRVertexBufAdd(unsigned int count);
inline void  _glKosVertexBufDecrement();
inline void  _glKosVertexBufReset();
inline unsigned int _glKosVertexBufCount(unsigned char list);
unsigned char _glKosList();
inline void _glKosVertexBufCopy(void *src, void *dst, GLuint count);
inline void _glKosResetEnabledTex();

/* Vertex Clip Buffer Internal Functions */
inline void *_glKosClipBufAddress();
inline void *_glKosClipBufPointer();
inline void  _glKosClipBufIncrement();
inline void  _glKosClipBufReset();

/* Vertex Array Buffer Internal Functions */
inline void      _glKosArrayBufIncrement();
inline void      _glKosArrayBufReset();
inline glVertex *_glKosArrayBufAddr();
inline glVertex *_glKosArrayBufPtr();

/* Initialize the OpenGL PVR Pipeline */
int  _glKosInitPVR();

/* Compile the current Polygon Header for the PVR */
void _glKosCompileHdr();
void _glKosCompileHdrTx();
void _glKosCompileHdrTx2();

/* Clipping Internal Functions */
void         _glKosTransformClipBuf(pvr_vertex_t *v, GLuint verts);
unsigned int _glKosClipTriangleStrip(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipTriangles(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipQuads(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices);

unsigned int _glKosClipTrianglesTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count);
unsigned char _glKosClipTriTransformed(pvr_vertex_t *vin, float *w, pvr_vertex_t *vout);
unsigned int _glKosClipQuadsTransformed(pvr_vertex_t *vin, float *w, pvr_vertex_t *vout, unsigned int vertices);
unsigned int _glKosClipTriangleStripTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count);

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
void _glKosVertex2ft(GLfloat x, GLfloat y);
void _glKosVertex2ftv(GLfloat *xy);
void _glKosVertex3ft(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3ftv(GLfloat *xyz);
void _glKosVertex3fc(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fcv(GLfloat *xyz);
void _glKosVertex3fp(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fpv(GLfloat *xyz);
void _glKosVertex3fl(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3flv(GLfloat *xyz);
void _glKosVertex3flc(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3flcv(GLfloat *xyz);
void _glKosVertex3fs(GLfloat x, GLfloat y, GLfloat z);
void _glKosVertex3fsv(GLfloat *xyz);

/* Matrix Internal Functions */
void _glKosInitMatrix();
void _glKosMatrixLoadModelView();
void _glKosMatrixLoadModelRot();
void _glKosMatrixApplyScreenSpace();
void _glKosMatrixApplyRender();
void _glKosMatrixLoadRender();

/* API Enabled Capabilities Internal Functions */
GLint   _glKosEnabledTexture2D();
GLubyte _glKosEnabledNearZClip();
GLubyte _glKosEnabledLighting();

/* RGB Pixel Colorspace Internal Functions */
uint16 __glKosAverageQuadPixelRGB565(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageQuadPixelARGB1555(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageQuadPixelARGB4444(uint16 p1, uint16 p2, uint16 p3, uint16 p4);
uint16 __glKosAverageBiPixelRGB565(uint16 p1, uint16 p2);
uint16 __glKosAverageBiPixelARGB1555(uint16 p1, uint16 p2);
uint16 __glKosAverageBiPixelARGB4444(uint16 p1, uint16 p2);

#endif
