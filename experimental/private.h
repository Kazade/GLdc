#ifndef PRIVATE_H
#define PRIVATE_H

#include "../include/gl.h"
#include "../gl-api.h"
#include "../containers/aligned_vector.h"
#include "../containers/named_array.h"


#define TRACE_ENABLED 0
#define TRACE() if(TRACE_ENABLED) {fprintf(stderr, "%s\n", __func__);}


typedef struct {
    unsigned int cmd[8];
} PVRCommand;

typedef struct {
    unsigned int flags;      /* Constant PVR_CMD_USERCLIP */
    unsigned int d1, d2, d3; /* Ignored for this type */
    unsigned int sx,         /* Start x */
             sy,         /* Start y */
             ex,         /* End x */
             ey;         /* End y */
} PVRTileClipCommand; /* Tile Clip command for the pvr */

typedef struct {
    unsigned int list_type;
    AlignedVector vector;
} PolyList;

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
} TextureObject;

PolyList *activePolyList();

void initAttributePointers();
void initContext();
pvr_poly_cxt_t* getPVRContext();
GLubyte _glKosInitTextures();
void updatePVRTextureContext(pvr_poly_cxt_t* context, TextureObject* tx1);
TextureObject* getTexture0();
TextureObject* getTexture1();
TextureObject* getBoundTexture();
GLboolean isBlendingEnabled();

#define PVR_VERTEX_BUF_SIZE 2560 * 256
#define MAX_TEXTURE_UNITS 2

#define CLAMP( X, MIN, MAX )  ( (X)<(MIN) ? (MIN) : ((X)>(MAX) ? (MAX) : (X)) )

#define mat_trans_fv12() { \
        __asm__ __volatile__( \
                              "fldi1 fr15\n" \
                              "ftrv	 xmtrx, fv12\n" \
                              "fldi1 fr14\n" \
                              "fdiv	 fr15, fr14\n" \
                              "fmul	 fr14, fr12\n" \
                              "fmul	 fr14, fr13\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z) \
                              : "0" (__x), "1" (__y), "2" (__z) \
                              : "fr15" ); \
    }

#endif // PRIVATE_H
