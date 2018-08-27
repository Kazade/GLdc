#include <stdio.h>
#include <string.h>

#include <dc/vec3f.h>
#include "private.h"

static GLfloat SCENE_AMBIENT [] = {0.2, 0.2, 0.2, 1.0};
static GLboolean VIEWER_IN_EYE_COORDINATES = GL_TRUE;
static GLenum COLOR_CONTROL = GL_SINGLE_COLOR;
static GLboolean TWO_SIDED_LIGHTING = GL_FALSE;
static GLenum COLOR_MATERIAL_MODE = GL_AMBIENT_AND_DIFFUSE;

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

        LIGHTS[i].is_directional = GL_FALSE;
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
        case GL_POSITION: {
            _matrixLoadModelView();
            memcpy(LIGHTS[idx].position, params, sizeof(GLfloat) * 4);

            LIGHTS[idx].is_directional = (params[3] == 0.0f) ? GL_TRUE : GL_FALSE;

            mat_trans_single4(
                LIGHTS[idx].position[0],
                LIGHTS[idx].position[1],
                LIGHTS[idx].position[2],
                LIGHTS[idx].position[3]
            );
        }
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

void glColorMaterial(GLenum face, GLenum mode) {
    if(face != GL_FRONT_AND_BACK) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return;
    }

    GLenum validModes[] = {GL_AMBIENT, GL_DIFFUSE, GL_AMBIENT_AND_DIFFUSE, GL_EMISSION, GL_SPECULAR, 0};

    if(_glCheckValidEnum(mode, validModes, __func__) != 0) {
        return;
    }

    COLOR_MATERIAL_MODE = mode;
}

static inline GLboolean isDiffuseColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_DIFFUSE || COLOR_MATERIAL_MODE == GL_AMBIENT_AND_DIFFUSE);
}

static inline GLboolean isAmbientColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_AMBIENT || COLOR_MATERIAL_MODE == GL_AMBIENT_AND_DIFFUSE);
}

static inline GLboolean isSpecularColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_SPECULAR);
}

inline void initVec3(struct vec3f* v, const GLfloat* src) {
    memcpy(v, src, sizeof(GLfloat) * 3);
}

/* Fast POW Implementation - Less accurate, but much faster than math.h */
#define EXP_A 184
#define EXP_C 16249

static float FEXP(float y) {
    union {
        float d;
        struct {
            short j, i;
        } n;
    } eco;
    eco.n.i = EXP_A * (y) + (EXP_C);
    eco.n.j = 0;
    return eco.d;
}

static float FLOG(float y) {
    int *nTemp = (int *)&y;
    y = (*nTemp) >> 16;
    return (y - EXP_C) / EXP_A;
}

static float FPOW(float b, float p) {
    return FEXP(FLOG(b) * p);
}

void _glCalculateLightingContribution(const GLint light, const GLfloat* pos, const GLfloat* normal, uint8_t* bgra, GLfloat* colour) __attribute__((optimize("fast-math")));
void _glCalculateLightingContribution(const GLint light, const GLfloat* pos, const GLfloat* normal, uint8_t* bgra, GLfloat* colour) {
    LightSource* l = &LIGHTS[light];

    struct vec3f L = {
        l->position[0],
        l->position[1],
        l->position[2]
    };

    if(!l->is_directional) {
        L.x -= pos[0];
        L.y -= pos[1];
        L.z -= pos[2];
    }

    struct vec3f N = {
        normal[0],
        normal[1],
        normal[2]
    };

    struct vec3f V = {
        pos[0],
        pos[1],
        pos[2]
    };

    GLfloat d;
    vec3f_length(L.x, L.y, L.z, d);

    GLfloat oneOverL = 1.0f / d;

    L.x *= oneOverL;
    L.y *= oneOverL;
    L.z *= oneOverL;

    vec3f_normalize(V.x, V.y, V.z);

    GLfloat NdotL, VdotN;
    vec3f_dot(N.x, N.y, N.z, L.x, L.y, L.z, NdotL);
    vec3f_dot(V.x, V.y, V.z, N.x, N.y, N.z, VdotN);

    GLfloat VdotR = VdotN - NdotL;
    GLfloat specularPower = FPOW(VdotR > 0 ? VdotR : 0, MATERIAL.exponent);

    GLboolean colorMaterial = _glIsColorMaterialEnabled();

    GLfloat mD [] = {
        (colorMaterial && isDiffuseColorMaterial()) ? ((GLfloat)bgra[R8IDX]) / 255.0f : MATERIAL.diffuse[0],
        (colorMaterial && isDiffuseColorMaterial()) ? ((GLfloat)bgra[G8IDX]) / 255.0f : MATERIAL.diffuse[1],
        (colorMaterial && isDiffuseColorMaterial()) ? ((GLfloat)bgra[B8IDX]) / 255.0f : MATERIAL.diffuse[2],
        (colorMaterial && isDiffuseColorMaterial()) ? ((GLfloat)bgra[A8IDX]) / 255.0f : MATERIAL.diffuse[3]
    };

    GLfloat mA [] = {
        (colorMaterial && isAmbientColorMaterial()) ? ((GLfloat)bgra[R8IDX]) / 255.0f : MATERIAL.ambient[0],
        (colorMaterial && isAmbientColorMaterial()) ? ((GLfloat)bgra[G8IDX]) / 255.0f : MATERIAL.ambient[1],
        (colorMaterial && isAmbientColorMaterial()) ? ((GLfloat)bgra[B8IDX]) / 255.0f : MATERIAL.ambient[2],
        (colorMaterial && isAmbientColorMaterial()) ? ((GLfloat)bgra[A8IDX]) / 255.0f : MATERIAL.ambient[3]
    };

    GLfloat mS [] = {
        (colorMaterial && isSpecularColorMaterial()) ? ((GLfloat)bgra[R8IDX]) / 255.0f : MATERIAL.specular[0],
        (colorMaterial && isSpecularColorMaterial()) ? ((GLfloat)bgra[G8IDX]) / 255.0f : MATERIAL.specular[1],
        (colorMaterial && isSpecularColorMaterial()) ? ((GLfloat)bgra[B8IDX]) / 255.0f : MATERIAL.specular[2],
        (colorMaterial && isSpecularColorMaterial()) ? ((GLfloat)bgra[A8IDX]) / 255.0f : MATERIAL.specular[3]
    };

    colour[0] = l->ambient[0] * mA[0];
    colour[1] = l->ambient[1] * mA[1];
    colour[2] = l->ambient[2] * mA[2];
    colour[3] = mD[3];

    if(NdotL >= 0) {
        colour[0] += (l->diffuse[0] * mD[0] * NdotL + l->specular[0] * mS[0] * specularPower);
        colour[1] += (l->diffuse[1] * mD[1] * NdotL + l->specular[1] * mS[1] * specularPower);
        colour[2] += (l->diffuse[2] * mD[2] * NdotL + l->specular[2] * mS[2] * specularPower);
    }

    if(!l->is_directional) {
        GLfloat att = (
            1.0f / (l->constant_attenuation + (l->linear_attenuation * d) + (l->quadratic_attenuation * d * d))
        );

        colour[0] *= att;
        colour[1] *= att;
        colour[2] *= att;
    }

    if(colour[0] > 1.0f) colour[0] = 1.0f;
    if(colour[1] > 1.0f) colour[1] = 1.0f;
    if(colour[2] > 1.0f) colour[2] = 1.0f;
    if(colour[3] > 1.0f) colour[3] = 1.0f;
}
