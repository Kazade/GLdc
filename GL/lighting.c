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

void _glInitLights() {
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
            _glMatrixLoadModelView();
            memcpy(LIGHTS[idx].position, params, sizeof(GLfloat) * 4);

            LIGHTS[idx].is_directional = (params[3] == 0.0f) ? GL_TRUE : GL_FALSE;
            if(LIGHTS[idx].is_directional) {
                //FIXME: Do we need to rotate directional lights?
            } else {
                mat_trans_single4(
                    LIGHTS[idx].position[0],
                    LIGHTS[idx].position[1],
                    LIGHTS[idx].position[2],
                    LIGHTS[idx].position[3]
                );
            }
        }
        break;
        case GL_SPOT_DIRECTION: {
            LIGHTS[idx].spot_direction[0] = params[0];
            LIGHTS[idx].spot_direction[1] = params[1];
            LIGHTS[idx].spot_direction[2] = params[2];
        } break;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
        case GL_SPOT_CUTOFF:
        case GL_SPOT_EXPONENT:
            glLightf(light, pname, *params);
        break;
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
            LIGHTS[idx].spot_exponent = param;
        break;
        case GL_SPOT_CUTOFF:
            LIGHTS[idx].spot_cutoff = param;
        break;
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

void APIENTRY glColorMaterial(GLenum face, GLenum mode) {
    if(face != GL_FRONT_AND_BACK) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return;
    }

    GLint validModes[] = {GL_AMBIENT, GL_DIFFUSE, GL_AMBIENT_AND_DIFFUSE, GL_EMISSION, GL_SPECULAR, 0};

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

static inline void initVec3(struct vec3f* v, const GLfloat* src) {
    memcpy(v, src, sizeof(GLfloat) * 3);
}

/* Fast POW Implementation - Less accurate, but much faster than math.h */
#define EXP_A 184
#define EXP_C 16249

static inline float FEXP(float y) {
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

static inline float FLOG(float y) {
    int *nTemp = (int *)&y;
    y = (*nTemp) >> 16;
    return (y - EXP_C) / EXP_A;
}

static inline float FPOW(float b, float p) {
    return FEXP(FLOG(b) * p);
}

void _glCalculateLighting(EyeSpaceData* ES, Vertex* vertex) {

    /* Before we begin, lets fiddle some pointers if COLOR_MATERIAL
     * is enabled */

    const GLboolean colorMaterial = _glIsColorMaterialEnabled();
    const GLboolean isDiffuseCM = isDiffuseColorMaterial();
    const GLboolean isAmbientCM = isAmbientColorMaterial();
    const GLboolean isSpecularCM = isSpecularColorMaterial();

    static GLfloat CM[4];

    if(colorMaterial) {
        CM[0] = ((GLfloat) vertex->bgra[R8IDX]) / 255.0f;
        CM[1] = ((GLfloat) vertex->bgra[G8IDX]) / 255.0f;
        CM[2] = ((GLfloat) vertex->bgra[B8IDX]) / 255.0f;
        CM[3] = ((GLfloat) vertex->bgra[A8IDX]) / 255.0f;
    }

    const GLfloat* MD = (colorMaterial && isDiffuseCM) ? CM : MATERIAL.diffuse;
    const GLfloat* MA = (colorMaterial && isAmbientCM) ? CM : MATERIAL.ambient;
    const GLfloat* MS = (colorMaterial && isSpecularCM) ? CM : MATERIAL.specular;

    /* Right..
     *
     * global propertie:
     *
     * acs - Global Ambient
     *
     * vertex-specific properties:
     *
     * ecm - Material Emission
     * acm - Material Ambient
     * dcm - Material Diffuse
     * n - Normal
     * V - Vertex Position
     * VPe - Vector from V to eye point (0, 0, 0, -1) basically negative V
     *
     * light-specifc properties:
     *
     * att - Attenution
     * acli - Light Ambient
     * Ppli - Light Position
     * dcli - Light Diffuse
     * fi - 1/0 facing light or not
     * VPpli - Vector from V to Ppli
     * ndotPpli - Dot product between n and Ppli
     * hi -
     * PpliV - vector from Ppli to V
     */


/* Each colour component is calculated in its own scope
 * so that the SH4 float registers don't get flooded */
#define LIGHT_COMPONENT(C) { \
    const GLfloat acm = MA[C]; \
    const GLfloat dcm = MD[C]; \
    const GLfloat scm = MS[C]; \
    const GLfloat scli = light->specular[C]; \
    const GLfloat dcli = light->diffuse[C]; \
    const GLfloat acli = light->ambient[C]; \
    const GLfloat srm = MATERIAL.exponent; \
\
    final[C] += (att * spot * ( \
        (acm * acli) + (ndotVPpli * dcm * dcli) + \
        (FPOW((fi * ndothi), srm) * scm * scli) \
    )); \
}

    const GLfloat* n = ES->n;
    const GLfloat* V = ES->xyz;

    GLfloat Vpe [] = {-V[0], -V[1], -V[2]};
    GLfloat VpeL;
    vec3f_length(Vpe[0], Vpe[1], Vpe[2], VpeL);
    Vpe[0] /= VpeL;
    Vpe[1] /= VpeL;
    Vpe[2] /= VpeL;

    GLfloat final[4] = {
        MATERIAL.emissive[0] + (MA[0] * SCENE_AMBIENT[0]),
        MATERIAL.emissive[1] + (MA[1] * SCENE_AMBIENT[1]),
        MATERIAL.emissive[2] + (MA[2] * SCENE_AMBIENT[2]),
        MD[3] // GL spec says alpha is always from the diffuse
    };

    GLubyte i;
    for(i = 0; i < MAX_LIGHTS; ++i) {
        if(!_glIsLightEnabled(i)) continue;

        const LightSource* light = &LIGHTS[i];

        const GLfloat* Ppli = light->position;
        GLfloat VPpli [] = {
            Ppli[0] - V[0],
            Ppli[1] - V[1],
            Ppli[2] - V[2]
        };

        GLfloat VPpliL;
        vec3f_length(VPpli[0], VPpli[1], VPpli[2], VPpliL);

        VPpli[0] /= VPpliL;
        VPpli[1] /= VPpliL;
        VPpli[2] /= VPpliL;

        GLfloat ndotVPpli;
        vec3f_dot(n[0], n[1], n[2], VPpli[0], VPpli[1], VPpli[2], ndotVPpli);

        const GLfloat k0 = light->constant_attenuation;
        const GLfloat k1 = light->linear_attenuation;
        const GLfloat k2 = light->quadratic_attenuation;
        const GLfloat att = (light->position[3] == 0) ? 1.0f : 1.0f / k0 + (k1 * VPpliL) + (k2 * VPpliL * VPpliL);
        const GLfloat spot = 1.0f; // FIXME: Spotlights

        const GLfloat fi = (ndotVPpli == 0) ? 0 : 1;

        GLfloat hi [3];
        if(!VIEWER_IN_EYE_COORDINATES) {
            // FIXME: Docs show power of T or something?
            hi[0] = VPpli[0] + 0;
            hi[1] = VPpli[1] + 0;
            hi[2] = VPpli[2] + 1;
        } else {
            hi[0] = VPpli[0] + Vpe[0];
            hi[1] = VPpli[1] + Vpe[1];
            hi[2] = VPpli[2] + Vpe[2];
        }

        GLfloat ndothi;
        vec3f_dot(n[0], n[1], n[2], hi[0], hi[1], hi[2], ndothi);

        LIGHT_COMPONENT(0);
        LIGHT_COMPONENT(1);
        LIGHT_COMPONENT(2);
    }

#undef LIGHT_COMPONENT

    vertex->bgra[R8IDX] = (GLubyte)(fminf(final[0] * 255.0f, 255.0f));
    vertex->bgra[G8IDX] = (GLubyte)(fminf(final[1] * 255.0f, 255.0f));
    vertex->bgra[B8IDX] = (GLubyte)(fminf(final[2] * 255.0f, 255.0f));
    vertex->bgra[A8IDX] = (GLubyte)(fminf(final[3] * 255.0f, 255.0f));
}

