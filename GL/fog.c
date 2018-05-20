
#include <limits.h>
#include "private.h"

static GLfloat FOG_START = 0.0f;
static GLfloat FOG_END = 1.0f;
static GLfloat FOG_DENSITY = 1.0f;
static GLenum FOG_MODE = GL_EXP;
static GLfloat FOG_COLOR [] = {0.0f, 0.0f, 0.0f, 0.0f};

static void updatePVRFog() {
    if(FOG_MODE == GL_LINEAR) {
        pvr_fog_table_linear(FOG_START, FOG_END);
    } else if(FOG_MODE == GL_EXP) {
        pvr_fog_table_exp(FOG_DENSITY);
    } else if(FOG_MODE == GL_EXP2) {
        pvr_fog_table_exp2(FOG_DENSITY);
    }
    pvr_fog_table_color(FOG_COLOR[3], FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2]);
}

void APIENTRY glFogf(GLenum pname,  GLfloat param) {
    switch(pname) {
    case GL_FOG_MODE: {
        FOG_MODE = (GLenum) param;
        updatePVRFog();
    } break;
    case GL_FOG_DENSITY: {
        FOG_DENSITY = param;
        updatePVRFog();
    } break;
    case GL_FOG_START: {
        FOG_START = param;
        updatePVRFog();
    } break;
    case GL_FOG_END: {
        FOG_END = param;
        updatePVRFog();
    } break;
    case GL_FOG_INDEX:
    default: {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
    }
}

void APIENTRY glFogi(GLenum pname,  GLint param) {
    glFogf(pname, (GLfloat) param);
}

void APIENTRY glFogfv(GLenum pname,  const GLfloat* params) {
    if(pname == GL_FOG_COLOR) {
        FOG_COLOR[0] = params[0];
        FOG_COLOR[1] = params[1];
        FOG_COLOR[2] = params[2];
        FOG_COLOR[3] = params[3];
        updatePVRFog();
    } else {
        glFogf(pname, *params);
    }
}

void APIENTRY glFogiv(GLenum pname,  const GLint* params) {
    if(pname == GL_FOG_COLOR) {
        FOG_COLOR[0] = ((GLfloat) params[0]) / (GLfloat) INT_MAX;
        FOG_COLOR[1] = ((GLfloat) params[1]) / (GLfloat) INT_MAX;
        FOG_COLOR[2] = ((GLfloat) params[2]) / (GLfloat) INT_MAX;
        FOG_COLOR[3] = ((GLfloat) params[3]) / (GLfloat) INT_MAX;
        updatePVRFog();
    } else {
        glFogi(pname, *params);
    }
}
