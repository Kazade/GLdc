
/*
 TODO: glGetX() fog values
*/

#include <limits.h>
#include "private.h"

static struct {
    GLfloat START;
    GLfloat END;
    GLfloat DENSITY;
    GLenum  MODE;
    GLfloat COLOR[4];
} FOG = {
    0.0f, 1.0f, 1.0f, GL_EXP, { 0.0f, 0.0f, 0.0f, 0.0f }
};

static void updatePVRFog(void) {
    switch(FOG.MODE) {
        case GL_LINEAR:
            GPUSetFogLinear(FOG.START, FOG.END);
            break;

        case GL_EXP:
            GPUSetFogExp(FOG.DENSITY);
            break;

        case GL_EXP2:
            GPUSetFogExp2(FOG.DENSITY);
            break;
    }

    GPUSetFogColor(FOG.COLOR[3], FOG.COLOR[0], FOG.COLOR[1], FOG.COLOR[2]);
}

void APIENTRY glFogf(GLenum pname, GLfloat param) {
    switch(pname) {
        case GL_FOG_DENSITY:
            if(FOG.DENSITY != param) {
                if(param < 0.0f)
                    _glKosThrowError(GL_INVALID_VALUE, __func__);
                else {
                    FOG.DENSITY = param;
                    updatePVRFog();
                }
            }
            break;

        case GL_FOG_START:
            if(FOG.START != param) {
                FOG.START = param;
                updatePVRFog();
            }
            break;

        case GL_FOG_END:
            if(FOG.END != param) {
                FOG.END = param;
                updatePVRFog();
            }
            break;

        default:
            _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glFogi(GLenum pname, GLint param) {
    switch(pname) {
        case GL_FOG_DENSITY:
        case GL_FOG_START:
        case GL_FOG_END:
            glFogf(pname, (GLfloat)param);
            break;

        case GL_FOG_MODE:
            if(FOG.MODE != param) {
                switch(param) {
                    case GL_LINEAR:
                    case GL_EXP:
                    case GL_EXP2:
                        FOG.MODE = param;
                        updatePVRFog();
                        break;

                    default:
                        _glKosThrowError(GL_INVALID_ENUM, __func__);
                }
            }
            break;

        case GL_FOG_INDEX:
        default:
            _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glFogfv(GLenum pname, const GLfloat* params) {
    switch(pname) {
        case GL_FOG_COLOR: {
            GLfloat color[] = {
                CLAMP(params[0], 0.0f, 1.0f),
                CLAMP(params[1], 0.0f, 1.0f),
                CLAMP(params[2], 0.0f, 1.0f),
                CLAMP(params[3], 0.0f, 1.0f)
            };

            if(memcmp(color, FOG.COLOR, sizeof(float) * 4)) {
                memcpy(FOG.COLOR, color, sizeof(float) * 4);
                updatePVRFog();
            }
            break;
        }

        default:
            glFogf(pname, *params);
    }
}

void APIENTRY glFogiv(GLenum pname, const GLint* params) {
    switch(pname) {
        case GL_FOG_COLOR: {
            GLfloat color[] = {
                (GLfloat)params[0] / (GLfloat)INT_MAX,
                (GLfloat)params[1] / (GLfloat)INT_MAX,
                (GLfloat)params[2] / (GLfloat)INT_MAX,
                (GLfloat)params[3] / (GLfloat)INT_MAX,
            };

            glFogfv(pname, color);
            break;
        }

        default:
            glFogi(pname, *params);
    }
}
