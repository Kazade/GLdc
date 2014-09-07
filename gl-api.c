/* KallistiGL for KallistiOS ##version##

   libgl/gl-api.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson
   Copyright (C) 2014 Lawrence Sebald

   Some functionality adapted from the original KOS libgl:
   Copyright (C) 2001 Dan Potter
   Copyright (C) 2002 Benoit Miller

   This API implements much but not all of the OpenGL 1.1 for KallistiOS.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl.h"
#include "glu.h"
#include "gl-api.h"
#include "gl-sh4.h"
#include "gl-pvr.h"

/* Texture Submission Functions ************************************************************************/
/* The texutre sumbission uses a fixed-size buffer, that defaults to 1024 textures.
   That means that you must manage your texture submission by calling glDelTextures(...)
   when you are done using a texture that was created by glGenTextures(...).
   The size of the texture buffer can be controlled by setting GL_MAX_TEXTURES on gl-pvr.h.

   glTexImage2D(..) is used for loading texture data to OpenGL that is located in Main RAM:
   OpenGL will allocate VRAM for the texture, and copy the texture data from Main RAM to VRAM.

   glKosTexImage2D(...) is used for loading texture data to OpenGL that is already in VRAM:
   This mode must be used for a render-to-texture if you are using glutSwapBuffersToTexture().
*/

typedef struct {
    GLenum    target;
    GLint     level;
    GLint     border;
    GLenum    format;
    GLenum    type;
    GLint     filter, env;
    GLuint    width, height;
    GLuint    internalFormat;
    GLvoid   *data;
} glTexCxt;

static glTexCxt GL_TEX_LIST[GL_MAX_TEXTURES];
static GLint    GL_BOUND_TEX = -1, GL_LAST_BOUND_TEX = -1,
                GL_BOUND_TEX1 = -1, GL_LAST_BOUND_TEX1 = -1;

static GLubyte BLEND_SRC       = PVR_BLEND_ONE;
static GLubyte BLEND_DST       = PVR_BLEND_ZERO;

static GLubyte GL_SHADE_FUNC   = PVR_SHADE_GOURAUD;

static GLubyte GL_CULL_FUNC    = PVR_CULLING_NONE;

static GLubyte GL_DEPTH_CMP    = PVR_DEPTHCMP_GEQUAL;
static GLubyte GL_DEPTH_ENABLE = PVR_DEPTHWRITE_ENABLE;

static GLubyte GL_TEX_ENV      = PVR_TXRENV_MODULATEALPHA;

static GLubyte GL_ENABLE_CLIPZ      = 0;
static GLubyte GL_ENABLE_LIGHTING   = 0;
static GLubyte GL_ENABLE_SCISSOR    = 0;
static GLubyte GL_ENABLE_FOG        = 0;
static GLubyte GL_ENABLE_CULLING    = 0;
static GLubyte GL_ENABLE_DEPTH_TEST = 0;
static GLubyte GL_ENABLE_SUPERSAMP  = 0;

static GLubyte GL_FACE_FRONT  = 0;
static GLubyte GL_CLAMP_ST    = 0;

/* Primitive 3D Position Submission */
void (*glVertex3f)(float, float, float);
void (*glVertex3fv)(float *);

#define GL_CLAMP_U  0x0F
#define GL_CLAMP_V  0xF0
#define GL_CLAMP_UV 0xFF

#define GL_TEXTURE_0 1<<0
#define GL_TEXTURE_1 1<<1

static GLuint GL_TEXTURE_ENABLED = 0;
static GLuint GL_CUR_ACTIVE_TEX = 0;

void glActiveTexture(GLenum texture) {
    if(texture < GL_TEXTURE0 || texture > GL_TEXTURE0 + GL_MAX_TEXTURE_UNITS)
        return;

    GL_CUR_ACTIVE_TEX = ((texture & 0xFF) - (GL_TEXTURE0 & 0xFF));
}

void glBindTexture(GLenum  target, GLuint texture) {
    if(target == GL_TEXTURE_2D) {
        switch(GL_CUR_ACTIVE_TEX) {
            case 0:
                GL_BOUND_TEX = texture;
                GL_LAST_BOUND_TEX = GL_BOUND_TEX;
                break;

            case 1:
                GL_BOUND_TEX1 = texture;
                GL_LAST_BOUND_TEX1 = GL_BOUND_TEX1;
                break;
        }
    }
}

void glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
        case GL_ACTIVE_TEXTURE:
            *params = GL_CUR_ACTIVE_TEX;
            break;

        case GL_BLEND:
            *params = _glKosList();
            break;

        case GL_BLEND_DST:
            *params = BLEND_SRC;
            break;

        case GL_BLEND_SRC:
            *params = BLEND_DST;
            break;

        case GL_CULL_FACE:
            *params = GL_ENABLE_CULLING;
            break;

        case GL_CULL_FACE_MODE:
            *params = GL_CULL_FUNC;
            break;

        case GL_DEPTH_FUNC:
            *params = GL_DEPTH_CMP;
            break;

        case GL_DEPTH_TEST:
            *params = GL_ENABLE_DEPTH_TEST;
            break;

        case GL_DEPTH_WRITEMASK:
            *params = GL_DEPTH_ENABLE;
            break;

        case GL_FRONT_FACE:
            *params = GL_FACE_FRONT;
            break;

        case GL_SCISSOR_TEST:
            *params = GL_ENABLE_SCISSOR;
            break;

        default:
            *params = -1; // Indicate invalid parameter //
            break;
    }
}

void glGetFloatv(GLenum pname, GLfloat *params) {
    switch(pname) {
        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX:
        case GL_TEXTURE_MATRIX:
            glKosGetMatrix(pname - GL_MODELVIEW_MATRIX + 1, params);
            break;

        default:
            *params = (GLfloat)GL_INVALID_ENUM;
            break;
    }
}

static void _glKosInitTextures() {
    GLuint i;

    for(i = 0; i < GL_MAX_TEXTURES; i++)
        GL_TEX_LIST[i].data = NULL;
}

static GLuint _glKosNextTexture() {
    GLint i;

    for(i = 0; i < GL_MAX_TEXTURES; i++)
        if(GL_TEX_LIST[i].data == NULL)
            return i;

    return 0; /* Invalid Texture! */
}

void glGenTextures(GLsizei n, GLuint *textures) {
    while(n--)
        *textures++ = _glKosNextTexture();
}

void glDelTextures(GLsizei n, GLuint *textures) {
    while(n--) {
        if(GL_TEX_LIST[*textures].data != NULL) {
            pvr_mem_free(GL_TEX_LIST[*textures].data);
            GL_TEX_LIST[*textures].data = NULL;
        }

        textures++;
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, GLvoid *data) {
    if(target != GL_TEXTURE_2D) return;

    GL_TEX_LIST[GL_BOUND_TEX].target = target;
    GL_TEX_LIST[GL_BOUND_TEX].level = level;
    GL_TEX_LIST[GL_BOUND_TEX].border = border;
    GL_TEX_LIST[GL_BOUND_TEX].format = format;
    GL_TEX_LIST[GL_BOUND_TEX].type = type;

    GLuint bytes;

    if(!level)
        bytes = width * height * 2;
    else
        bytes = glKosMipMapTexSize(width, height);

    if(format == PVR_TXRFMT_VQ_ENABLE)
        bytes /= 4;

    GL_TEX_LIST[GL_BOUND_TEX].data = pvr_mem_malloc(bytes);

    GL_TEX_LIST[GL_BOUND_TEX].width = width;
    GL_TEX_LIST[GL_BOUND_TEX].height = height;
    GL_TEX_LIST[GL_BOUND_TEX].internalFormat = internalFormat;

    GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_NONE;

    sq_cpy(GL_TEX_LIST[GL_BOUND_TEX].data, data, bytes);
}

void glKosTexImage2D(GLenum target, GLint level, GLint internalFormat,
                     GLsizei width, GLsizei height, GLint border,
                     GLenum format, GLenum type, GLvoid *data) {
    if(target != GL_TEXTURE_2D) return;

    GL_TEX_LIST[GL_BOUND_TEX].target = target;
    GL_TEX_LIST[GL_BOUND_TEX].level = level;
    GL_TEX_LIST[GL_BOUND_TEX].border = border;
    GL_TEX_LIST[GL_BOUND_TEX].format = format;
    GL_TEX_LIST[GL_BOUND_TEX].type = type;

    GL_TEX_LIST[GL_BOUND_TEX].data = data;

    GL_TEX_LIST[GL_BOUND_TEX].width = width;
    GL_TEX_LIST[GL_BOUND_TEX].height = height;
    GL_TEX_LIST[GL_BOUND_TEX].internalFormat = internalFormat;

    GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_NONE;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if(target == GL_TEXTURE_2D) {
        switch(pname) {
            case GL_TEXTURE_MAG_FILTER:
            case GL_TEXTURE_MIN_FILTER:
                switch(param) {
                    case GL_LINEAR:
                        GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_BILINEAR;
                        break;

                    case GL_NEAREST:
                        GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_NEAREST;
                        break;

                    case GL_FILTER_NONE: /* Compatabile with deprecated kgl */
                        GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_NONE;
                        break;

                    case GL_FILTER_BILINEAR: /* Compatabile with deprecated kgl */
                        GL_TEX_LIST[GL_BOUND_TEX].filter = PVR_FILTER_BILINEAR;
                        break;

                    case GL_NEAREST_MIPMAP_NEAREST:
                        GL_TEX_LIST[GL_BOUND_TEX].level = PVR_MIPMAP_ENABLE;
                        break;

                    default:
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_S:
                switch(param) {
                    case GL_CLAMP:
                        GL_CLAMP_ST |= GL_CLAMP_U;
                        break;

                    case GL_REPEAT:
                        GL_CLAMP_ST ^= GL_CLAMP_U;
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_T:
                switch(param) {
                    case GL_CLAMP:
                        GL_CLAMP_ST |= GL_CLAMP_V;
                        break;

                    case GL_REPEAT:
                        GL_CLAMP_ST ^= GL_CLAMP_V;
                        break;
                }

                break;
        }
    }
}

void glTexEnvi(GLenum target, GLenum pname, GLint param) {
    if(target == GL_TEXTURE_ENV)
        if(pname == GL_TEXTURE_ENV_MODE)
            if(param >= PVR_TXRENV_REPLACE && param <= PVR_TXRENV_MODULATEALPHA)
                GL_TEX_ENV = param;
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    /* GL_TEXTURE_LOD_BIAS Not Implemented */
    glTexEnvi(target, pname, param);
}

/* Blending / Shading functions ********************************************************/

void glShadeModel(GLenum   mode) {
    switch(mode) {
        case GL_FLAT:
            GL_SHADE_FUNC = PVR_SHADE_FLAT;
            break;

        case GL_SMOOTH:
            GL_SHADE_FUNC = PVR_SHADE_GOURAUD;
            break;
    }
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    switch(sfactor) {
        case GL_ONE:
            BLEND_SRC = PVR_BLEND_ONE;
            break;

        case GL_ZERO:
            BLEND_SRC = PVR_BLEND_ZERO;
            break;

        case GL_SRC_COLOR:
            BLEND_SRC = PVR_BLEND_SRCALPHA;
            break;

        case GL_DST_COLOR:
            BLEND_SRC = PVR_BLEND_DESTCOLOR;
            break;

        case GL_SRC_ALPHA:
            BLEND_SRC = PVR_BLEND_SRCALPHA;
            break;

        case GL_DST_ALPHA:
            BLEND_SRC = PVR_BLEND_DESTALPHA;
            break;

        case GL_ONE_MINUS_SRC_ALPHA:
            BLEND_SRC = PVR_BLEND_INVSRCALPHA;
            break;

        case GL_ONE_MINUS_DST_ALPHA:
            BLEND_SRC = PVR_BLEND_INVDESTALPHA;
            break;

        case GL_ONE_MINUS_DST_COLOR:
            BLEND_SRC = PVR_BLEND_INVDESTCOLOR;
            break;
    }

    switch(dfactor) {
        case GL_ONE:
            BLEND_DST = PVR_BLEND_ONE;
            break;

        case GL_ZERO:
            BLEND_DST = PVR_BLEND_ZERO;
            break;

        case GL_SRC_COLOR:
            BLEND_DST = PVR_BLEND_SRCALPHA;
            break;

        case GL_DST_COLOR:
            BLEND_DST = PVR_BLEND_DESTCOLOR;
            break;

        case GL_SRC_ALPHA:
            BLEND_DST = PVR_BLEND_SRCALPHA;
            break;

        case GL_DST_ALPHA:
            BLEND_DST = PVR_BLEND_DESTALPHA;
            break;

        case GL_ONE_MINUS_SRC_ALPHA:
            BLEND_DST = PVR_BLEND_INVSRCALPHA;
            break;

        case GL_ONE_MINUS_DST_ALPHA:
            BLEND_DST = PVR_BLEND_INVDESTALPHA;
            break;

        case GL_ONE_MINUS_DST_COLOR:
            BLEND_DST = PVR_BLEND_INVDESTCOLOR;
            break;
    }
}

/* Depth / Clear functions */
static GLfloat GL_DEPTH_CLEAR = 1.0f,
               GL_COLOR_CLEAR[3] = { 0, 0, 0 };

void glClear(GLuint mode) {
    if(mode & GL_COLOR_BUFFER_BIT)
        pvr_set_bg_color(GL_COLOR_CLEAR[0], GL_COLOR_CLEAR[1], GL_COLOR_CLEAR[2]);
}

void glClearColor(float r, float g, float b, float a) {
    if(r > 1) r = 1;

    if(g > 1) g = 1;

    if(b > 1) b = 1;

    if(a > 1) a = 1;

    GL_COLOR_CLEAR[0] = r * a;
    GL_COLOR_CLEAR[1] = g * a;
    GL_COLOR_CLEAR[2] = b * a;
}

void glClearDepthf(GLfloat depth) {
    GL_DEPTH_CLEAR = depth;
}

void glDepthFunc(GLenum func) {
    switch(func) {
        case GL_LESS:
            GL_DEPTH_CMP = PVR_DEPTHCMP_GEQUAL;
            break;

        case GL_LEQUAL:
            GL_DEPTH_CMP = PVR_DEPTHCMP_GREATER;
            break;

        case GL_GREATER:
            GL_DEPTH_CMP = PVR_DEPTHCMP_LEQUAL;
            break;

        case GL_GEQUAL:
            GL_DEPTH_CMP = PVR_DEPTHCMP_LESS;
            break;

        default:
            GL_DEPTH_CMP = func & 0xF;
    }
}

void glDepthMask(GLboolean flag) {
    flag ? (GL_DEPTH_ENABLE = PVR_DEPTHWRITE_ENABLE) :
    (GL_DEPTH_ENABLE = PVR_DEPTHWRITE_DISABLE);
}

void glFrontFace(GLenum mode) {
    switch(mode) {
        case GL_CW:
        case GL_CCW:
            GL_FACE_FRONT = mode;
            break;
    }
}

void glCullFace(GLenum mode) {
    switch(mode) {
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            GL_CULL_FUNC = mode;
            break;
    }
}

/* glEnable / glDisable ****************************************************************/

void glEnable(GLenum cap) {
    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT7) return _glKosEnableLight(cap);

    switch(cap) {
        case GL_TEXTURE_2D:
            GL_TEXTURE_ENABLED |= (1 << GL_CUR_ACTIVE_TEX);
            break;

        case GL_BLEND:
            _glKosVertexBufSwitchTR();
            break;

        case GL_DEPTH_TEST:
            GL_ENABLE_DEPTH_TEST = 1;
            break;

        case GL_LIGHTING:
            GL_ENABLE_LIGHTING = 1;
            break;

        case GL_KOS_NEARZ_CLIPPING:
            GL_ENABLE_CLIPZ = 1;
            break;

        case GL_SCISSOR_TEST:
            GL_ENABLE_SCISSOR = 1;
            break;

        case GL_FOG:
            GL_ENABLE_FOG = 1;
            break;

        case GL_CULL_FACE:
            GL_ENABLE_CULLING  = 1;
            break;
    }
}

void glDisable(GLenum cap) {
    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT7) return _glKosDisableLight(cap);

    switch(cap) {
        case GL_TEXTURE_2D:
            GL_TEXTURE_ENABLED &= ~(1 << GL_CUR_ACTIVE_TEX);
            break;

        case GL_BLEND:
            _glKosVertexBufSwitchOP();
            break;

        case GL_DEPTH_TEST:
            GL_ENABLE_DEPTH_TEST = 0;
            break;

        case GL_LIGHTING:
            GL_ENABLE_LIGHTING = 0;
            break;

        case GL_KOS_NEARZ_CLIPPING:
            GL_ENABLE_CLIPZ = 0;
            break;

        case GL_SCISSOR_TEST:
            GL_ENABLE_SCISSOR = 0;
            break;

        case GL_FOG:
            GL_ENABLE_FOG = 0;
            break;

        case GL_CULL_FACE:
            GL_ENABLE_CULLING = 0;
            break;
    }
}

/* Vertex Submission Functions *************************************************************************/
static GLuint  GL_VERTICES = 0,
               GL_VERTEX_MODE = GL_TRIANGLES;
static GLuint  GL_VERTEX_COLOR = 0xFFFFFFFF;
//static GLuint  GL_VERTEX_SPECULAR = 0xFF000000;
static GLfloat GL_VERTEX_UV[2] = {0, 0};

typedef struct {
    GLfloat u;
    GLfloat v;
} TexCoord;

static TexCoord GL_MULTI_UV[GL_MAX_VERTS];

static pvr_poly_cxt_t GL_POLY_CXT;

void glKosInit() {
    _glKosInitPVR();

    _glKosInitTextures();

    _glKosInitMatrix();

    _glKosInitLighting();
}

void _glKosCompileHdr() {
    pvr_poly_hdr_t *hdr = _glKosVertexBufPointer();

    /* Non-textured pvr poly header */
    pvr_poly_cxt_col(&GL_POLY_CXT, _glKosList() * 2);

    GL_POLY_CXT.gen.shading = GL_SHADE_FUNC;

    if(GL_ENABLE_DEPTH_TEST)
        GL_POLY_CXT.depth.comparison = GL_DEPTH_CMP;
    else
        GL_POLY_CXT.depth.comparison = PVR_DEPTHCMP_ALWAYS;

    GL_POLY_CXT.depth.write = GL_DEPTH_ENABLE;

    if(GL_ENABLE_SCISSOR)
        GL_POLY_CXT.gen.clip_mode = PVR_USERCLIP_INSIDE;

    if(GL_ENABLE_FOG)
        GL_POLY_CXT.gen.fog_type = PVR_FOG_TABLE;

    if(GL_ENABLE_CULLING) {
        if(GL_CULL_FUNC == GL_BACK) {
            if(GL_FACE_FRONT == GL_CW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
        else if(GL_CULL_FUNC == GL_FRONT) {
            if(GL_FACE_FRONT == GL_CCW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
    }
    else
        GL_POLY_CXT.gen.culling = PVR_CULLING_NONE;

    pvr_poly_compile(hdr, &GL_POLY_CXT);

    if(GL_ENABLE_SUPERSAMP)
        hdr->mode2 |= GL_PVR_SAMPLE_SUPER << PVR_TA_SUPER_SAMPLE_SHIFT;

    _glKosVertexBufIncrement();
}

void _glKosCompileHdrTx() {
    pvr_poly_hdr_t *hdr = _glKosVertexBufPointer();

    pvr_poly_cxt_txr(&GL_POLY_CXT, _glKosList() * 2,
                     GL_TEX_LIST[GL_BOUND_TEX].format |
                     GL_TEX_LIST[GL_BOUND_TEX].type,
                     GL_TEX_LIST[GL_BOUND_TEX].width,
                     GL_TEX_LIST[GL_BOUND_TEX].height,
                     GL_TEX_LIST[GL_BOUND_TEX].data,
                     GL_TEX_LIST[GL_BOUND_TEX].filter);

    GL_POLY_CXT.gen.shading = GL_SHADE_FUNC;

    if(GL_ENABLE_DEPTH_TEST)
        GL_POLY_CXT.depth.comparison = GL_DEPTH_CMP;
    else
        GL_POLY_CXT.depth.comparison = PVR_DEPTHCMP_ALWAYS;

    GL_POLY_CXT.depth.write = GL_DEPTH_ENABLE;

    if(GL_ENABLE_SCISSOR)
        GL_POLY_CXT.gen.clip_mode = PVR_USERCLIP_INSIDE;

    if(GL_ENABLE_FOG)
        GL_POLY_CXT.gen.fog_type = PVR_FOG_TABLE;

    if(GL_ENABLE_CULLING) {
        if(GL_CULL_FUNC == GL_BACK) {
            if(GL_FACE_FRONT == GL_CW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
        else if(GL_CULL_FUNC == GL_FRONT) {
            if(GL_FACE_FRONT == GL_CCW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
    }
    else
        GL_POLY_CXT.gen.culling = PVR_CULLING_NONE;

    if(GL_CLAMP_ST == GL_CLAMP_UV)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_UV;
    else if(GL_CLAMP_ST & GL_CLAMP_U)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_U;
    else if(GL_CLAMP_ST & GL_CLAMP_V)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_V;

    GL_POLY_CXT.txr.mipmap      = GL_TEX_LIST[GL_BOUND_TEX].level;
    GL_POLY_CXT.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;

    if(_glKosList()) {
        GL_POLY_CXT.txr.env = GL_TEX_ENV;
        GL_POLY_CXT.blend.src = BLEND_SRC;
        GL_POLY_CXT.blend.dst = BLEND_DST;
    }

    pvr_poly_compile(hdr, &GL_POLY_CXT);

    if(GL_ENABLE_SUPERSAMP)
        hdr->mode2 |= GL_PVR_SAMPLE_SUPER << PVR_TA_SUPER_SAMPLE_SHIFT;

    _glKosVertexBufIncrement();
}


void _glKosCompileHdrTx2() {
    pvr_poly_hdr_t *hdr = _glKosTRVertexBufPointer();

    pvr_poly_cxt_txr(&GL_POLY_CXT, 2,
                     GL_TEX_LIST[GL_BOUND_TEX1].format |
                     GL_TEX_LIST[GL_BOUND_TEX1].type,
                     GL_TEX_LIST[GL_BOUND_TEX1].width,
                     GL_TEX_LIST[GL_BOUND_TEX1].height,
                     GL_TEX_LIST[GL_BOUND_TEX1].data,
                     GL_TEX_LIST[GL_BOUND_TEX1].filter);

    GL_POLY_CXT.gen.shading = GL_SHADE_FUNC;

    if(GL_ENABLE_DEPTH_TEST)
        GL_POLY_CXT.depth.comparison = GL_DEPTH_CMP;
    else
        GL_POLY_CXT.depth.comparison = PVR_DEPTHCMP_ALWAYS;

    GL_POLY_CXT.depth.write = GL_DEPTH_ENABLE;

    if(GL_ENABLE_SCISSOR)
        GL_POLY_CXT.gen.clip_mode = PVR_USERCLIP_INSIDE;

    if(GL_ENABLE_FOG)
        GL_POLY_CXT.gen.fog_type = PVR_FOG_TABLE;

    if(GL_ENABLE_CULLING) {
        if(GL_CULL_FUNC == GL_BACK) {
            if(GL_FACE_FRONT == GL_CW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
        else if(GL_CULL_FUNC == GL_FRONT) {
            if(GL_FACE_FRONT == GL_CCW)
                GL_POLY_CXT.gen.culling = PVR_CULLING_CCW;
            else
                GL_POLY_CXT.gen.culling = PVR_CULLING_CW;
        }
    }
    else
        GL_POLY_CXT.gen.culling = PVR_CULLING_NONE;

    if(GL_CLAMP_ST == GL_CLAMP_UV)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_UV;
    else if(GL_CLAMP_ST & GL_CLAMP_U)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_U;
    else if(GL_CLAMP_ST & GL_CLAMP_V)
        GL_POLY_CXT.txr.uv_clamp = PVR_UVCLAMP_V;

    GL_POLY_CXT.txr.mipmap      = GL_TEX_LIST[GL_BOUND_TEX1].level;
    GL_POLY_CXT.txr.mipmap_bias = PVR_MIPBIAS_NORMAL;

    GL_POLY_CXT.txr.env = GL_TEX_ENV;
    GL_POLY_CXT.blend.src = BLEND_SRC;
    GL_POLY_CXT.blend.dst = BLEND_DST;

    pvr_poly_compile(hdr, &GL_POLY_CXT);

    if(GL_ENABLE_SUPERSAMP)
        hdr->mode2 |= GL_PVR_SAMPLE_SUPER << PVR_TA_SUPER_SAMPLE_SHIFT;

    _glKosTRVertexBufIncrement();
}

/* Vertex Color Submission */

void glColor1ui(GLuint argb) {
    GL_VERTEX_COLOR = argb;
}

void glColor4ub(GLubyte r, GLubyte  g, GLubyte b, GLubyte a) {
    GL_VERTEX_COLOR = a << 24 | r << 16 | g << 8 | b;
}

void glColor3f(float r, float g, float b) {
    GL_VERTEX_COLOR = PVR_PACK_COLOR(1.0f, r, g, b);
}

void glColor3fv(float *rgb) {
    GL_VERTEX_COLOR = PVR_PACK_COLOR(1.0f, rgb[0], rgb[1], rgb[2]);
}

void glColor4f(float r, float g, float b, float a) {
    GL_VERTEX_COLOR = PVR_PACK_COLOR(a, r, g, b);
}

void glColor4fv(float *rgba) {
    GL_VERTEX_COLOR = PVR_PACK_COLOR(rgba[3], rgba[0], rgba[1], rgba[2]);
}

/* Texture Coordinate Submission */

void glTexCoord2f(GLfloat u, GLfloat v) {
    GL_VERTEX_UV[0] = u;
    GL_VERTEX_UV[1] = v;
}

void glTexCoord2fv(GLfloat *uv) {
    GL_VERTEX_UV[0] = uv[0];
    GL_VERTEX_UV[1] = uv[1];
}

void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
    switch(target) {
        case GL_TEXTURE0:
            GL_VERTEX_UV[0] = s;
            GL_VERTEX_UV[1] = t;
            break;

        case GL_TEXTURE1:
            GL_MULTI_UV[GL_VERTICES].u = s;
            GL_MULTI_UV[GL_VERTICES].v = t;
            break;
    }
}

void glMultiTexCoord2fv(GLenum target, const GLfloat *v) {
    switch(target) {
        case GL_TEXTURE0:
            GL_VERTEX_UV[0] = v[0];
            GL_VERTEX_UV[1] = v[1];
            break;

        case GL_TEXTURE1:
            GL_MULTI_UV[GL_VERTICES].u = v[0];
            GL_MULTI_UV[GL_VERTICES].v = v[1];
            break;
    }
}

static inline void _glKosMultiTexCoord(pvr_vertex_t *v, GLuint count) {
    GLuint i;

    for(i = 0; i < count; i++) {
        v[i].u = GL_MULTI_UV[i].u;
        v[i].v = GL_MULTI_UV[i].v;
    }
}

static inline void _glKosMultiTexCoordQuads(pvr_vertex_t *v, GLuint count) {
    GLuint i;

    for(i = 0; i < count / 4; i += 4) {
        v[i + 0].u = GL_MULTI_UV[i + 0].u;
        v[i + 0].v = GL_MULTI_UV[i + 0].v;
        v[i + 1].u = GL_MULTI_UV[i + 1].u;
        v[i + 1].v = GL_MULTI_UV[i + 1].v;
        v[i + 2].u = GL_MULTI_UV[i + 3].u;
        v[i + 2].v = GL_MULTI_UV[i + 3].v;
        v[i + 3].u = GL_MULTI_UV[i + 2].u;
        v[i + 3].v = GL_MULTI_UV[i + 2].v;
    }
}

/* Vertex Submission */

static inline void _glKosVertexSwap(pvr_vertex_t *v1, pvr_vertex_t *v2) {
    pvr_vertex_t tmp = *v1;
    *v1 = *v2;
    *v2 = * &tmp;
}

static inline void _glKosFlagsSetQuad() {
    pvr_vertex_t *v = (pvr_vertex_t *)_glKosVertexBufPointer() - GL_VERTICES;
    GLuint i;

    for(i = 0; i < GL_VERTICES; i += 4) {
        _glKosVertexSwap(v + 2, v + 3);
        v->flags = (v + 1)->flags = (v + 2)->flags = PVR_CMD_VERTEX;
        (v + 3)->flags = PVR_CMD_VERTEX_EOL;
        v += 4;
    }
}

static inline void _glKosFlagsSetTriangle() {
    pvr_vertex_t *v = (pvr_vertex_t *)_glKosVertexBufPointer() - GL_VERTICES;
    GLuint i;

    for(i = 0; i < GL_VERTICES; i += 3) {
        v->flags = (v + 1)->flags = PVR_CMD_VERTEX;
        (v + 2)->flags = PVR_CMD_VERTEX_EOL;
        v += 3;
    }
}

static inline void _glKosFlagsSetTriangleStrip() {
    pvr_vertex_t *v = (pvr_vertex_t *)_glKosVertexBufPointer() - GL_VERTICES;
    GLuint i;

    for(i = 0; i < GL_VERTICES - 1; i++) {
        v->flags = PVR_CMD_VERTEX;
        v++;
    }

    v->flags = PVR_CMD_VERTEX_EOL;
}

void glBegin(unsigned int mode) {
    _glKosMatrixApplyRender(); /* Apply the Render Matrix Stack */

    _glKosArrayBufReset();  /* Make sure arrays buffer is reset */

    GL_VERTEX_MODE = mode;

    !GL_TEXTURE_ENABLED ? _glKosCompileHdr() : _glKosCompileHdrTx();

    if(GL_TEXTURE_ENABLED & GL_TEXTURE_1)
        _glKosCompileHdrTx2();

    GL_VERTICES = 0;

    if(mode == GL_POINTS) {
        glVertex3f = _glKosVertex3fp;
        glVertex3fv = _glKosVertex3fpv;
    }
    else if(GL_ENABLE_CLIPZ && GL_ENABLE_LIGHTING) {
        glVertex3f = _glKosVertex3flc;
        glVertex3fv = _glKosVertex3flcv;
    }
    else if(GL_ENABLE_LIGHTING) {
        glVertex3f = _glKosVertex3fl;
        glVertex3fv = _glKosVertex3flv;
    }
    else if(GL_ENABLE_CLIPZ) {
        glVertex3f = _glKosVertex3fc;
        glVertex3fv = _glKosVertex3fcv;
    }
    else {
        glVertex3f = _glKosVertex3ft;
        glVertex3fv = _glKosVertex3ftv;
    }
}

void _glKosTransformClipBuf(pvr_vertex_t *v, GLuint verts) {
    register float __x  __asm__("fr12");
    register float __y  __asm__("fr13");
    register float __z  __asm__("fr14");

    while(verts--) {
        __x = v->x;
        __y = v->y;
        __z = v->z;

        mat_trans_fv12();

        v->x = __x;
        v->y = __y;
        v->z = __z;
        ++v;
    }
}

static inline void _glKosCopyMultiTexture(pvr_vertex_t *v, GLuint count) {
    if(GL_TEXTURE_ENABLED & GL_TEXTURE_1) {
        _glKosVertexBufCopy(_glKosTRVertexBufPointer(), v, count);

        if(GL_VERTEX_MODE == GL_QUADS)
            _glKosMultiTexCoordQuads(_glKosTRVertexBufPointer(), count);
        else
            _glKosMultiTexCoord(_glKosTRVertexBufPointer(), count);

        _glKosTRVertexBufAdd(count);
    }
}

void glEnd() {
    if(GL_ENABLE_CLIPZ) { /* Z-Clipping Enabled */
        if(GL_ENABLE_LIGHTING) {
            _glKosVertexComputeLighting(_glKosClipBufAddress(), GL_VERTICES);

            _glKosMatrixLoadRender();
        }

        GLuint cverts;
        pvr_vertex_t *v = _glKosVertexBufPointer();

        switch(GL_VERTEX_MODE) {
            case GL_TRIANGLES:
                cverts = _glKosClipTriangles(_glKosClipBufAddress(), v, GL_VERTICES);
                _glKosTransformClipBuf(v, cverts);
                _glKosCopyMultiTexture(v, cverts);
                _glKosVertexBufAdd(cverts);
                break;

            case GL_TRIANGLE_STRIP:
                cverts = _glKosClipTriangleStrip(_glKosClipBufAddress(), v, GL_VERTICES);
                _glKosTransformClipBuf(v, cverts);
                _glKosCopyMultiTexture(v, cverts);
                _glKosVertexBufAdd(cverts);
                break;

            case GL_QUADS:
                cverts = _glKosClipQuads(_glKosClipBufAddress(), v, GL_VERTICES);
                _glKosTransformClipBuf(v, cverts);
                _glKosCopyMultiTexture(v, cverts);
                _glKosVertexBufAdd(cverts);
                break;
        }

        _glKosClipBufReset();
    }
    else { /* No Z-Clipping Enabled */
        if(GL_ENABLE_LIGHTING)
            _glKosVertexComputeLighting((pvr_vertex_t *)_glKosVertexBufPointer() - GL_VERTICES, GL_VERTICES);

        switch(GL_VERTEX_MODE) {
            case GL_TRIANGLES:
                _glKosFlagsSetTriangle();
                break;

            case GL_TRIANGLE_STRIP:
                _glKosFlagsSetTriangleStrip();
                break;

            case GL_QUADS:
                _glKosFlagsSetQuad();
                break;
        }

        _glKosCopyMultiTexture((pvr_vertex_t *)_glKosVertexBufPointer() - GL_VERTICES, GL_VERTICES);
    }
}

void glVertex2f(GLfloat x, GLfloat y) {
    return _glKosVertex3ft(x, y, 0.0f);
}

void glVertex2fv(GLfloat *xy) {
	return _glKosVertex3ft(xy[0], xy[1], 0.0f);
}

void glKosVertex2f(GLfloat x, GLfloat y) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    v->x = x;
    v->y = y;
    v->z = 10;
    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void glKosVertex2fv(GLfloat *xy) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    v->x = xy[0];
    v->y = xy[1];
    v->z = 10;
    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3fs(float x, float y, float z) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(x, y, z, v->x, v->y, v->z);

    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3fsv(float *xyz) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(xyz[0], xyz[1], xyz[2], v->x, v->y, v->z);

    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3ft(float x, float y, float z) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(x, y, z, v->x, v->y, v->z);

    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3ftv(float *xyz) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(xyz[0], xyz[1], xyz[2], v->x, v->y, v->z);

    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosVertexBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3fc(float x, float y, float z) {
    pvr_vertex_t *v = _glKosClipBufPointer();

    v->x = x;
    v->y = y;
    v->z = z;
    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosClipBufIncrement();

    ++GL_VERTICES;
}

void _glKosVertex3fcv(float *xyz) {
    pvr_vertex_t *v = _glKosClipBufPointer();

    v->x = xyz[0];
    v->y = xyz[1];
    v->z = xyz[2];
    v->u = GL_VERTEX_UV[0];
    v->v = GL_VERTEX_UV[1];
    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;

    _glKosClipBufIncrement();

    ++GL_VERTICES;
}

/* GL_POINTS */
static float GL_POINT_SIZE = 0.02;

inline void _glKosVertex3fpa(float x, float y, float z) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(x, y, z, v->x, v->y, v->z);

    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;
    v->flags = PVR_CMD_VERTEX;

    _glKosVertexBufIncrement();
}

inline void _glKosVertex3fpb(float x, float y, float z) {
    pvr_vertex_t *v = _glKosVertexBufPointer();

    mat_trans_single3_nomod(x, y, z, v->x, v->y, v->z);

    v->argb  = GL_VERTEX_COLOR;
    //v->oargb = GL_VERTEX_SPECULAR;
    v->flags = PVR_CMD_VERTEX_EOL;

    _glKosVertexBufIncrement();
}

void _glKosVertex3fp(float x, float y, float z) {
    _glKosVertex3fpa(x - GL_POINT_SIZE, y - GL_POINT_SIZE, z);
    _glKosVertex3fpa(x + GL_POINT_SIZE, y - GL_POINT_SIZE, z);
    _glKosVertex3fpa(x - GL_POINT_SIZE, y + GL_POINT_SIZE, z);
    _glKosVertex3fpb(x + GL_POINT_SIZE, y + GL_POINT_SIZE, z);
}

void _glKosVertex3fpv(float *xyz) {
    _glKosVertex3fpa(xyz[0] - GL_POINT_SIZE, xyz[1] - GL_POINT_SIZE, xyz[2]);
    _glKosVertex3fpa(xyz[0] + GL_POINT_SIZE, xyz[1] - GL_POINT_SIZE, xyz[2]);
    _glKosVertex3fpa(xyz[0] - GL_POINT_SIZE, xyz[1] + GL_POINT_SIZE, xyz[2]);
    _glKosVertex3fpb(xyz[0] + GL_POINT_SIZE, xyz[1] + GL_POINT_SIZE, xyz[2]);
}

/* Misc. functions */

/* Clamp X to [MIN,MAX]: */
#define CLAMP( X, MIN, MAX )  ( (X)<(MIN) ? (MIN) : ((X)>(MAX) ? (MAX) : (X)) )

/* Setup the hardware user clip rectangle.

   The minimum clip rectangle is a 32x32 area which is dependent on the tile
   size use by the tile accelerator. The PVR swithes off rendering to tiles
   outside or inside the defined rectangle dependant upon the 'clipmode'
   bits in the polygon header.

   Clip rectangles therefore must have a size that is some multiple of 32.

    glScissor(0, 0, 32, 32) allows only the 'tile' in the lower left
    hand corner of the screen to be modified and glScissor(0, 0, 0, 0)
    disallows modification to all 'tiles' on the screen.
*/
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    pvr_cmd_tclip_t *c = _glKosVertexBufPointer();

    GLint miny, maxx, maxy;
    GLsizei gl_scissor_width = CLAMP(width, 0, vid_mode->width);
    GLsizei gl_scissor_height = CLAMP(height, 0, vid_mode->height);

    /* force the origin to the lower left-hand corner of the screen */
    miny = (vid_mode->height - gl_scissor_height) - y;
    maxx = (gl_scissor_width + x);
    maxy = (gl_scissor_height + miny);

    /* load command structure while mapping screen coords to TA tiles */
    c->flags = PVR_CMD_USERCLIP;
    c->d1 = c->d2 = c->d3 = 0;
    c->sx = CLAMP(x / 32, 0, vid_mode->width / 32);
    c->sy = CLAMP(miny / 32, 0, vid_mode->height / 32);
    c->ex = CLAMP((maxx / 32) - 1, 0, vid_mode->width / 32);
    c->ey = CLAMP((maxy / 32) - 1, 0, vid_mode->height / 32);

    _glKosVertexBufIncrement();
}

void glHint(GLenum target, GLenum mode) {
    switch(target) {
        case GL_PERSPECTIVE_CORRECTION_HINT:
            if(mode == GL_NICEST)
                GL_ENABLE_SUPERSAMP = 1;
            else
                GL_ENABLE_SUPERSAMP = 0;

            break;
    }
}

GLint _glKosEnabledTexture2D() {
    return GL_TEXTURE_ENABLED;
}

GLubyte _glKosEnabledNearZClip() {
    return GL_ENABLE_CLIPZ;
}

GLubyte _glKosEnabledLighting() {
    return GL_ENABLE_LIGHTING;
}

inline void _glKosResetEnabledTex() {
    GL_TEXTURE_ENABLED = GL_CUR_ACTIVE_TEX = 0;
}
