/* KallistiGL for KallistiOS ##version##

   libgl/gl-cap.c
   Copyright (C) 2014 Josh Pearson

    KOS Open GL Capabilty State Machine Implementation.
*/

#include "gl.h"
#include "gl-api.h"

#include <malloc.h>
#include <stdio.h>

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

static GLbitfield GL_KOS_ENABLE_CAP = 0;

//===============================================================================//
//== External API Functions ==//

void APIENTRY glEnable(GLenum cap) {
    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT15) return _glKosEnableLight(cap);

    switch(cap) {
        case GL_TEXTURE_2D:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_TEXTURE2D;
            break;

        case GL_BLEND:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_BLENDING;
            _glKosVertexBufSwitchTR();
            break;

        case GL_DEPTH_TEST:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_DEPTH_TEST;
            break;

        case GL_LIGHTING:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_LIGHTING;
            break;

        case GL_KOS_NEARZ_CLIPPING:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_ZCLIPPING;
            break;

        case GL_SCISSOR_TEST:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_SCISSOR_TEST;
            break;

        case GL_FOG:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_FOG;
            break;

        case GL_CULL_FACE:
            GL_KOS_ENABLE_CAP |= GL_KOS_ENABLE_CULLING;
            break;
    }
}

void APIENTRY glDisable(GLenum cap) {
    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT15) return _glKosDisableLight(cap);

    switch(cap) {
        case GL_TEXTURE_2D:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_TEXTURE2D;
            break;

        case GL_BLEND:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_BLENDING;
            _glKosVertexBufSwitchOP();
            break;

        case GL_DEPTH_TEST:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_DEPTH_TEST;
            break;

        case GL_LIGHTING:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_LIGHTING;
            break;

        case GL_KOS_NEARZ_CLIPPING:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_ZCLIPPING;
            break;

        case GL_SCISSOR_TEST:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_SCISSOR_TEST;
            break;

        case GL_FOG:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_FOG;
            break;

        case GL_CULL_FACE:
            GL_KOS_ENABLE_CAP &= ~GL_KOS_ENABLE_CULLING;
            break;
    }
}

GLboolean APIENTRY glIsEnabled(GLenum cap) {
    if(cap >= GL_LIGHT0 && cap <= GL_LIGHT15) return _glKosIsLightEnabled(cap & 0xFF);

    switch(cap) {
        case GL_DEPTH_TEST:
            return _glKosEnabledDepthTest() ? GL_TRUE : GL_FALSE;

        case GL_SCISSOR_TEST:
            return _glKosEnabledScissorTest() ? GL_TRUE : GL_FALSE;

        case GL_CULL_FACE:
            return _glKosEnabledFog() ? GL_TRUE : GL_FALSE;

        case GL_FOG:
            return _glKosEnabledFog() ? GL_TRUE : GL_FALSE;

        case GL_LIGHTING:
            return _glKosEnabledLighting() ? GL_TRUE : GL_FALSE;

        case GL_KOS_NEARZ_CLIPPING:
            return _glKosEnabledNearZClip() ? GL_TRUE : GL_FALSE;

        case GL_TEXTURE_2D:
            return _glKosEnabledTexture2D() ? GL_TRUE : GL_FALSE;

        case GL_BLEND:
            return _glKosEnabledBlend() ? GL_TRUE : GL_FALSE;
    }

    return GL_FALSE;
}

void APIENTRY glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
        case GL_ACTIVE_TEXTURE:
            *params = 0;
            break;

        case GL_BLEND:
            *params = _glKosList();
            break;

        case GL_BLEND_DST:
            *params = _glKosBlendSrcFunc();
            break;

        case GL_BLEND_SRC:
            *params = _glKosBlendDstFunc();
            break;

        case GL_CULL_FACE:
            *params = _glKosEnabledCulling();
            break;

        case GL_CULL_FACE_MODE:
            *params = _glKosCullFaceMode();
            break;

        case GL_DEPTH_FUNC:
            *params = _glKosDepthFunc();
            break;

        case GL_DEPTH_TEST:
            *params = _glKosEnabledDepthTest();
            break;

        case GL_DEPTH_WRITEMASK:
            *params = _glKosDepthMask();
            break;

        case GL_FRONT_FACE:
            *params = _glKosCullFaceFront();
            break;

        case GL_SCISSOR_TEST:
            *params = _glKosEnabledScissorTest();
            break;

        case GL_MAX_LIGHTS:
            *params = _glKosGetMaxLights();
            break;

        case GL_TEXTURE_BINDING_2D:
            *params = _glKosBoundTexID();
            break;

        default:
            *params = -1; // Indicate invalid parameter //
            break;
    }
}

void APIENTRY glGetFloatv(GLenum pname, GLfloat *params) {
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