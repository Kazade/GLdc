#include "../include/gl.h"

static pvr_poly_cxt_t GL_CONTEXT;

/* We can't just use the GL_CONTEXT for this state as the two
 * GL states are combined, so we store them separately and then
 * calculate the appropriate PVR state from them. */
static GLenum CULL_FACE = GL_BACK;
static GLenum FRONT_FACE = GL_CCW;
static GLboolean CULLING_ENABLED = GL_FALSE;

static int _calc_pvr_face_culling() {
    if(!CULLING_ENABLED) {
        return PVR_CULLING_NONE;
    } else {
        if(CULL_FACE == GL_BACK) {
            return (FRONT_FACE == GL_CW) ? PVR_CULLING_CCW : PVR_CULLING_CW;
        } else {
            return (FRONT_FACE == GL_CCW) ? PVR_CULLING_CCW : PVR_CULLING_CW;
        }
    }
}

static GLenum DEPTH_FUNC = GL_LESS;
static GLboolean DEPTH_TEST_ENABLED = GL_FALSE;

static int _calc_pvr_depth_test() {
    if(!DEPTH_TEST_ENABLED) {
        return PVR_DEPTH_CMP_ALWAYS;
    }

    switch(func) {
        case GL_NEVER:
            return PVR_DEPTH_CMP_NEVER;
        case GL_LESS:
            return PVR_DEPTH_CMP_LESS;
        case GL_EQUAL:
            return PVR_DEPTH_CMP_EQUAL;
        case GL_LEQUAL:
            return PVR_DEPTH_CMP_LEQUAL;
        case GL_GREATER:
            return PVR_DEPTH_CMP_GREATER;
        case GL_NOTEQUAL:
            return PVR_DEPTH_CMP_NOTEQUAL;
        case GL_GEQUAL:
            return PVR_DEPTH_CMP_GEQUAL;
        break;
        case GL_ALWAYS:
        default:
            return PVR_DEPTH_CMP_ALWAYS;
    }
}


void initContext() {
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glCullFace(GL_CCW);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

GLAPI void APIENTRY glEnable(GLenum cap) {
    switch(cap) {
        case GL_TEXTURE_2D:
            GL_CONTEXT.txr.enable = 1;
        break;
        case GL_CULL_FACE: {
            CULLING_ENABLED = GL_TRUE;
            GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
        } break;
        case GL_DEPTH_TEST: {
            DEPTH_TEST_ENABLED = GL_TRUE;
            GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
        } break;
    }
}

GLAPI void APIENTRY glDisable(GLenum cap) {
    switch(cap) {
        case GL_TEXTURE_2D: {
            GL_CONTEXT.txr.enabled = 0;
        } break;
        case GL_CULL_FACE: {
            CULLING_ENABLED = GL_FALSE;
            GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
        } break;
        case GL_DEPTH_TEST: {
            DEPTH_TEST_ENABLED = GL_FALSE;
            GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
        } break;
    }
}

/* Clear Caps */
GLAPI void APIENTRY glClear(GLuint mode) {

}

GLAPI void APIENTRY glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {

}

/* Depth Testing */
GLAPI void APIENTRY glClearDepthf(GLfloat depth) {

}

GLAPI void APIENTRY glClearDepth(GLfloat depth) {

}

GLAPI void APIENTRY glDepthMask(GLboolean flag) {
    GL_CONTEXT.depth.write = (flag == GL_TRUE) ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
}

GLAPI void APIENTRY glDepthFunc(GLenum func) {
    DEPTH_FUNC = func;
    GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
}

/* Hints */
/* Currently Supported Capabilities:
      GL_PERSPECTIVE_CORRECTION_HINT - This will Enable Texture Super-Sampling on the PVR */
GLAPI void APIENTRY glHint(GLenum target, GLenum mode) {

}

/* Culling */
GLAPI void APIENTRY glFrontFace(GLenum mode) {
    FRONT_FACE = mode;
}

GLAPI void APIENTRY glCullFace(GLenum mode) {
    CULL_FACE = mode;
}

/* Shading - Flat or Goraud */
GLAPI void APIENTRY glShadeModel(GLenum mode) {

}

/* Blending */
GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {

}

/* Texturing */
GLAPI void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {

}

GLAPI void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) {

}

GLAPI void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {

}

GLAPI void APIENTRY glGenTextures(GLsizei n, GLuint *textures) {

}

GLAPI void APIENTRY glDeleteTextures(GLsizei n, GLuint *textures) {

}

GLAPI void APIENTRY glBindTexture(GLenum  target, GLuint texture) {

}
