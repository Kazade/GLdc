#include "private.h"

static GLfloat SCENE_AMBIENT [] = {0.2, 0.2, 0.2, 1.0};
static GLboolean VIEWER_IN_EYE_COORDINATES = GL_TRUE;
static GLenum COLOR_CONTROL = GL_SINGLE_COLOR;
static GLboolean TWO_SIDED_LIGHTING = GL_FALSE;

static LightSource LIGHTS[MAX_LIGHTS];
static Material MATERIAL;

void initLights() {
    static GLfloat ONE [] = {1.0f, 1.0f, 1.0f, 1.0f};
    static GLfloat ZERO [] = {0.0f, 0.0f, 0.0f, 1.0f};
    static GLfloat PARTIAL [] = {0.2f, 0.2f, 0.2f, 1.0f};
    static GLfloat MOSTLY [] = {0.8f, 0.8f, 0.8f, 1.0f};

    memcpy(MATERIAL.ambient, PARTIAL, sizeof(GLfloat) * 4);
    memcpy(MATERIAL.diffuse, MOSTLY, sizeof(GLfloat) * 4);
    memcpy(MATERIAL.specular, ZERO, sizeof(GLfloat) * 4);
    memcpy(MATERIAL.emissive, ZERO, sizeof(GLfloat) * 4);
    MATERIAL.exponent = 0.0f;
    MATERIAL.ambient_color_index = 0.0f;
    MATERIAL.diffuse_color_index = 1.0f;
    MATERIAL.specular_color_index = 1.0f;

    GLubyte i;
    for(i = 0; i < MAX_LIGHTS; ++i) {
        memcpy(LIGHTS[i].ambient, ZERO, sizeof(GLfloat) * 4);
        memcpy(LIGHTS[i].diffuse, ONE, sizeof(GLfloat) * 4);
        memcpy(LIGHTS[i].specular, ONE, sizeof(GLfloat) * 4);

        if(i > 0) {
            memcpy(LIGHTS[i].diffuse, ZERO, sizeof(GLfloat) * 4);
            memcpy(LIGHTS[i].specular, ZERO, sizeof(GLfloat) * 4);
        }

        LIGHTS[i].position[0] = LIGHTS[i].position[1] = LIGHTS[i].position[3] = 0.0f;
        LIGHTS[i].position[2] = 1.0f;

        LIGHTS[i].spot_direction[0] = LIGHTS[i].spot_direction[1] = 0.0f;
        LIGHTS[i].spot_direction[2] = -1.0f;

        LIGHTS[i].spot_exponent = 0.0f;
        LIGHTS[i].spot_cutoff = 180.0f;

        LIGHTS[i].constant_attenuation = 1.0f;
        LIGHTS[i].linear_attenuation = 0.0f;
        LIGHTS[i].quadratic_attenuation = 0.0f;
    }
}

void APIENTRY glLightModelf(GLenum pname, const GLfloat param) {
    glLightModelfv(pname, &param);
}

void APIENTRY glLightModeli(GLenum pname, const GLint param) {
    glLightModeliv(pname, &param);
}

void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
        case GL_LIGHT_MODEL_AMBIENT:
            memcpy(SCENE_AMBIENT, params, sizeof(GLfloat) * 4);
        break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            VIEWER_IN_EYE_COORDINATES = (*params) ? GL_TRUE : GL_FALSE;
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        /* Not implemented */
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
}

void APIENTRY glLightModeliv(GLenum pname, const GLint* params) {
    switch(pname) {
        case GL_LIGHT_MODEL_COLOR_CONTROL:
            COLOR_CONTROL = *params;
        break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            VIEWER_IN_EYE_COORDINATES = (*params) ? GL_TRUE : GL_FALSE;
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
}

void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    GLubyte idx = light & 0xF;

    if(idx >= MAX_LIGHTS) {
        return;
    }

    switch(pname) {
        case GL_AMBIENT:
            memcpy(LIGHTS[idx].ambient, params, sizeof(GLfloat) * 4);
        break;
        case GL_DIFFUSE:
            memcpy(LIGHTS[idx].diffuse, params, sizeof(GLfloat) * 4);
        break;
        case GL_SPECULAR:
            memcpy(LIGHTS[idx].specular, params, sizeof(GLfloat) * 4);
        break;
        case GL_POSITION:
            memcpy(LIGHTS[idx].position, params, sizeof(GLfloat) * 4);
        break;
        case GL_CONSTANT_ATTENUATION:            
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
        case GL_SPOT_CUTOFF:
        case GL_SPOT_DIRECTION:
        case GL_SPOT_EXPONENT:
            glLightf(light, pname, *params);
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
}

void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param) {
    GLubyte idx = light & 0xF;

    if(idx >= MAX_LIGHTS) {
        return;
    }

    switch(pname) {
        case GL_CONSTANT_ATTENUATION:
            LIGHTS[idx].constant_attenuation = param;
        break;
        case GL_LINEAR_ATTENUATION:
            LIGHTS[idx].linear_attenuation = param;
        break;
        case GL_QUADRATIC_ATTENUATION:
            LIGHTS[idx].quadratic_attenuation = param;
        break;
        case GL_SPOT_EXPONENT:
        case GL_SPOT_CUTOFF:
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
}

void APIENTRY glMaterialf(GLenum face, GLenum pname, const GLfloat param) {
    if(face == GL_BACK || pname != GL_SHININESS) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return;
    }

    MATERIAL.exponent = param;
}

void APIENTRY glMateriali(GLenum face, GLenum pname, const GLint param) {
    glMaterialf(face, pname, param);
}

void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    if(pname == GL_SHININESS) {
        glMaterialf(face, pname, *params);
        return;
    }

    if(face == GL_BACK) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return;
    }

    switch(pname) {
        case GL_AMBIENT:
            memcpy(MATERIAL.ambient, params, sizeof(GLfloat) * 4);
        break;
        case GL_DIFFUSE:
            memcpy(MATERIAL.diffuse, params, sizeof(GLfloat) * 4);
        break;
        case GL_SPECULAR:
            memcpy(MATERIAL.specular, params, sizeof(GLfloat) * 4);
        break;
        case GL_EMISSION:
            memcpy(MATERIAL.specular, params, sizeof(GLfloat) * 4);
        break;
        case GL_AMBIENT_AND_DIFFUSE: {
            glMaterialfv(face, GL_AMBIENT, params);
            glMaterialfv(face, GL_DIFFUSE, params);
        } break;
        case GL_COLOR_INDEXES:
        default: {
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            _glKosPrintError();
        }
    }
}

void calculateLightingContribution(const GLint light, const GLfloat* pos, const GLfloat* normal, GLfloat* colour) {
    colour[0] = colour[3] = 0.0f;
    colour[1] = colour[2] = 0.0f;
}
