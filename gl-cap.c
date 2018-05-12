/* KallistiGL for KallistiOS ##version##

   libgl/gl-cap.c
   Copyright (C) 2014 Josh Pearson

    KOS Open GL Capabilty State Machine Implementation.
*/

#include "gl.h"
#include "gl-api.h"

//===============================================================================//
//== Enable Bit Flags ==//

#define GL_KOS_ENABLE_DEPTH_TEST   (1<<0)
#define GL_KOS_ENABLE_SCISSOR_TEST (1<<1)
#define GL_KOS_ENABLE_CULLING      (1<<2)
#define GL_KOS_ENABLE_FOG          (1<<3)
#define GL_KOS_ENABLE_LIGHTING     (1<<4)
#define GL_KOS_ENABLE_ZCLIPPING    (1<<5)
#define GL_KOS_ENABLE_SUPERSAMPLE  (1<<6)
#define GL_KOS_ENABLE_TEXTURE2D    (1<<7)
#define GL_KOS_ENABLE_BLENDING     (1<<8)
#define GL_KOS_ENABLE_TEXTURE_MAT  (1<<9)

static GLbitfield GL_KOS_ENABLE_CAP = 0;

//===============================================================================//
//== External API Functions ==//

//void APIENTRY glEnable(GLenum cap) {
//    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT15) return _glKosEnableLight(cap);

//    switch(cap) {
//        case GL_TEXTURE_2D:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_TEXTURE2D;
//            break;

//        case GL_BLEND:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_BLENDING;
//            _glKosVertexBufSwitchTR();
//            break;

//        case GL_DEPTH_TEST:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_DEPTH_TEST;
//            break;

//        case GL_LIGHTING:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_LIGHTING;
//            break;

//        case GL_KOS_NEARZ_CLIPPING:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_ZCLIPPING;
//            break;

//        case GL_SCISSOR_TEST:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_SCISSOR_TEST;
//            break;

//        case GL_FOG:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_FOG;
//            break;

//        case GL_CULL_FACE:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_CULLING;
//            break;

//        case GL_KOS_TEXTURE_MATRIX:
//            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_TEXTURE_MAT;
//            break;
//    }
//}

//void APIENTRY glDisable(GLenum cap) {
//    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT15) return _glKosDisableLight(cap);

//    switch(cap) {
//        case GL_TEXTURE_2D:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_TEXTURE2D;
//            break;

//        case GL_BLEND:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_BLENDING;
//            _glKosVertexBufSwitchOP();
//            break;

//        case GL_DEPTH_TEST:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_DEPTH_TEST;
//            break;

//        case GL_LIGHTING:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_LIGHTING;
//            break;

//        case GL_KOS_NEARZ_CLIPPING:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_ZCLIPPING;
//            break;

//        case GL_SCISSOR_TEST:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_SCISSOR_TEST;
//            break;

//        case GL_FOG:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_FOG;
//            break;

//        case GL_CULL_FACE:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_CULLING;
//            break;

//        case GL_KOS_TEXTURE_MATRIX:
//            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_TEXTURE_MAT;
//            break;
//    }
//}


void APIENTRY glGetFloatv(GLenum pname, GLfloat *params) {
    switch(pname) {
        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX:
        case GL_TEXTURE_MATRIX:
            glKosGetMatrix(pname - GL_MODELVIEW_MATRIX + 1, params);
            break;

        default:
            _glKosThrowError(GL_INVALID_ENUM, "glGetFloatv");
            _glKosPrintError();
            break;
    }
}

const GLbyte *glGetString(GLenum name) {
    switch(name) {
        case GL_VENDOR:
            return "KallistiOS / Kazade";

        case GL_RENDERER:
            return "PowerVR2 CLX2 100mHz";

        case GL_VERSION:
            return "KGL 1.x";

        case GL_EXTENSIONS:
            return "GL_ARB_framebuffer_object, GL_ARB_multitexture, GL_ARB_texture_rg";
    }

    return "GL_KOS_ERROR: ENUM Unsupported\n";
}

//===============================================================================//
//== Internal API Functions ==//

GLubyte _glKosEnabledDepthTest() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_DEPTH_TEST;
}

GLubyte _glKosEnabledScissorTest() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_SCISSOR_TEST;
}

GLubyte _glKosEnabledCulling() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_CULLING;
}

GLubyte _glKosEnabledFog() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_FOG;
}

GLubyte _glKosEnabledLighting() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_LIGHTING;
}

GLubyte _glKosEnabledNearZClip() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_ZCLIPPING;
}

GLubyte _glKosEnabledTexture2D() {
    return GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_TEXTURE2D;
}

GLubyte _glKosEnabledBlend() {
    return (GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_BLENDING) >> 8;
}

GLubyte _glKosEnabledTextureMatrix() {
    return (GL_KOS_ENABLE_CAP & GL_KOS_ENABLE_TEXTURE_MAT) >> 9;
}
