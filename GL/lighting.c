#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"

#define _MIN(x, y) (x < y) ? x : y

/* Lighting will not be calculated if the attenuation
 * multiplier ends up less than this value */
#define ATTENUATION_THRESHOLD 100.0f


void _glPrecalcLightingValues(GLuint mask) {
    /* Pre-calculate lighting values */
    GLshort i;

    Material* material = _glActiveMaterial();

    if(mask & AMBIENT_MASK) {
        for(i = 0; i < MAX_GLDC_LIGHTS; ++i) {
            LightSource* light = _glLightAt(i);

            light->ambientMaterial[0] = light->ambient[0] * material->ambient[0];
            light->ambientMaterial[1] = light->ambient[1] * material->ambient[1];
            light->ambientMaterial[2] = light->ambient[2] * material->ambient[2];
            light->ambientMaterial[3] = light->ambient[3] * material->ambient[3];

        }
    }

    if(mask & DIFFUSE_MASK) {
        for(i = 0; i < MAX_GLDC_LIGHTS; ++i) {
            LightSource* light = _glLightAt(i);

            light->diffuseMaterial[0] = light->diffuse[0] * material->diffuse[0];
            light->diffuseMaterial[1] = light->diffuse[1] * material->diffuse[1];
            light->diffuseMaterial[2] = light->diffuse[2] * material->diffuse[2];
            light->diffuseMaterial[3] = light->diffuse[3] * material->diffuse[3];
        }
    }

    if(mask & SPECULAR_MASK) {
        for(i = 0; i < MAX_GLDC_LIGHTS; ++i) {
            LightSource* light = _glLightAt(i);

            light->specularMaterial[0] = light->specular[0] * material->specular[0];
            light->specularMaterial[1] = light->specular[1] * material->specular[1];
            light->specularMaterial[2] = light->specular[2] * material->specular[2];
            light->specularMaterial[3] = light->specular[3] * material->specular[3];
        }
    }

    /* If ambient or emission are updated, we need to update
     * the base colour. */
    if((mask & AMBIENT_MASK) || (mask & EMISSION_MASK) || (mask & SCENE_AMBIENT_MASK)) {
        GLfloat* scene_ambient = _glLightModelSceneAmbient();

        material->baseColour[0] = MATH_fmac(scene_ambient[0], material->ambient[0], material->emissive[0]);
        material->baseColour[1] = MATH_fmac(scene_ambient[1], material->ambient[1], material->emissive[1]);
        material->baseColour[2] = MATH_fmac(scene_ambient[2], material->ambient[2], material->emissive[2]);
        material->baseColour[3] = MATH_fmac(scene_ambient[3], material->ambient[3], material->emissive[3]);
    }
}

void _glInitLights() {
    Material* material = _glActiveMaterial();

    static GLfloat ONE [] = {1.0f, 1.0f, 1.0f, 1.0f};
    static GLfloat ZERO [] = {0.0f, 0.0f, 0.0f, 1.0f};
    static GLfloat PARTIAL [] = {0.2f, 0.2f, 0.2f, 1.0f};
    static GLfloat MOSTLY [] = {0.8f, 0.8f, 0.8f, 1.0f};

    memcpy(material->ambient, PARTIAL, sizeof(GLfloat) * 4);
    memcpy(material->diffuse, MOSTLY, sizeof(GLfloat) * 4);
    memcpy(material->specular, ZERO, sizeof(GLfloat) * 4);
    memcpy(material->emissive, ZERO, sizeof(GLfloat) * 4);
    material->exponent = 0.0f;

    GLubyte i;
    for(i = 0; i < MAX_GLDC_LIGHTS; ++i) {
        LightSource* light = _glLightAt(i);

        memcpy(light->ambient, ZERO, sizeof(GLfloat) * 4);
        memcpy(light->diffuse, ONE, sizeof(GLfloat) * 4);
        memcpy(light->specular, ONE, sizeof(GLfloat) * 4);

        if(i > 0) {
            memcpy(light->diffuse, ZERO, sizeof(GLfloat) * 4);
            memcpy(light->specular, ZERO, sizeof(GLfloat) * 4);
        }

        light->position[0] = light->position[1] = light->position[3] = 0.0f;
        light->position[2] = 1.0f;
        light->isDirectional = GL_TRUE;
        light->isEnabled = GL_FALSE;

        light->spot_direction[0] = light->spot_direction[1] = 0.0f;
        light->spot_direction[2] = -1.0f;

        light->spot_exponent = 0.0f;
        light->spot_cutoff = 180.0f;

        light->constant_attenuation = 1.0f;
        light->linear_attenuation = 0.0f;
        light->quadratic_attenuation = 0.0f;
    }

    _glPrecalcLightingValues(~0);
    _glRecalcEnabledLights();
}


void APIENTRY glLightModelf(GLenum pname, const GLfloat param) {
    glLightModelfv(pname, &param);
}

void APIENTRY glLightModeli(GLenum pname, const GLint param) {
    glLightModeliv(pname, &param);
}

void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
        case GL_LIGHT_MODEL_AMBIENT: {
            if(memcmp(_glGetLightModelSceneAmbient(), params, sizeof(float) * 4) != 0) {
                _glSetLightModelSceneAmbient(params);
                _glPrecalcLightingValues(SCENE_AMBIENT_MASK);
            }
        } break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            _glSetLightModelViewerInEyeCoordinates((*params) ? GL_TRUE : GL_FALSE);
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        /* Not implemented */
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glLightModeliv(GLenum pname, const GLint* params) {
    switch(pname) {
        case GL_LIGHT_MODEL_COLOR_CONTROL:
            _glSetLightModelColorControl(*params);
        break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            _glSetLightModelViewerInEyeCoordinates((*params) ? GL_TRUE : GL_FALSE);
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    GLubyte idx = light & 0xF;

    if(idx >= MAX_GLDC_LIGHTS) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    GLuint mask = (pname == GL_AMBIENT) ? AMBIENT_MASK :
                  (pname == GL_DIFFUSE) ? DIFFUSE_MASK :
                  (pname == GL_SPECULAR) ? SPECULAR_MASK : 0;

    LightSource* l = _glLightAt(idx);

    GLboolean rebuild = GL_FALSE;

    switch(pname) {
        case GL_AMBIENT:
            rebuild = memcmp(l->ambient, params, sizeof(GLfloat) * 4) != 0;
            if(rebuild) {
                memcpy(l->ambient, params, sizeof(GLfloat) * 4);
            }
        break;
        case GL_DIFFUSE:
            rebuild = memcmp(l->diffuse, params, sizeof(GLfloat) * 4) != 0;
            if(rebuild) {
                memcpy(l->diffuse, params, sizeof(GLfloat) * 4);
            }
        break;
        case GL_SPECULAR:
            rebuild = memcmp(l->specular, params, sizeof(GLfloat) * 4) != 0;
            if(rebuild) {
                memcpy(l->specular, params, sizeof(GLfloat) * 4);
            }
        break;
        case GL_POSITION: {
            memcpy(l->position, params, sizeof(GLfloat) * 4);

            l->isDirectional = params[3] == 0.0f;

            if(l->isDirectional) {
                //FIXME: Do we need to rotate directional lights?
            } else {
                _glMatrixLoadModelView();
                TransformVec3(l->position);
            }
        }
        break;
        case GL_SPOT_DIRECTION: {
            l->spot_direction[0] = params[0];
            l->spot_direction[1] = params[1];
            l->spot_direction[2] = params[2];
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
        return;
    }

    if(rebuild) {
        _glPrecalcLightingValues(mask);
    }

}

void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param) {
    GLubyte idx = light & 0xF;

    if(idx >= MAX_GLDC_LIGHTS) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    LightSource* l = _glLightAt(idx);
    switch(pname) {
        case GL_CONSTANT_ATTENUATION:
            l->constant_attenuation = param;
        break;
        case GL_LINEAR_ATTENUATION:
            l->linear_attenuation = param;
        break;
        case GL_QUADRATIC_ATTENUATION:
            l->quadratic_attenuation = param;
        break;
        case GL_SPOT_EXPONENT:
            l->spot_exponent = param;
        break;
        case GL_SPOT_CUTOFF:
            l->spot_cutoff = param;
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
    }
}

void APIENTRY glMaterialf(GLenum face, GLenum pname, const GLfloat param) {
    if(face == GL_BACK || pname != GL_SHININESS) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    _glActiveMaterial()->exponent = _MIN(param, 128);  /* 128 is the max according to the GL spec */
}

void APIENTRY glMateriali(GLenum face, GLenum pname, const GLint param) {
    glMaterialf(face, pname, param);
}

void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    if(face == GL_BACK) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    Material* material = _glActiveMaterial();

    GLboolean rebuild = GL_FALSE;

    switch(pname) {
        case GL_SHININESS:
            glMaterialf(face, pname, *params);
            rebuild = GL_TRUE;
        break;
        case GL_AMBIENT: {
            if(memcmp(material->ambient, params, sizeof(float) * 4) != 0) {
                vec4cpy(material->ambient, params);
                rebuild = GL_TRUE;
            }
        } break;
        case GL_DIFFUSE:
            if(memcmp(material->diffuse, params, sizeof(float) * 4) != 0) {
                vec4cpy(material->diffuse, params);
                rebuild = GL_TRUE;
            }
        break;
        case GL_SPECULAR:
            if(memcmp(material->specular, params, sizeof(float) * 4) != 0) {
                vec4cpy(material->specular, params);
                rebuild = GL_TRUE;
            }
        break;
        case GL_EMISSION:
            if(memcmp(material->emissive, params, sizeof(float) * 4) != 0) {
                vec4cpy(material->emissive, params);
                rebuild = GL_TRUE;
            }
        break;
        case GL_AMBIENT_AND_DIFFUSE: {
            rebuild = (
                memcmp(material->ambient, params, sizeof(float) * 4) != 0 ||
                memcmp(material->diffuse, params, sizeof(float) * 4) != 0
            );

            if(rebuild) {
                vec4cpy(material->ambient, params);
                vec4cpy(material->diffuse, params);
            }
        } break;
        case GL_COLOR_INDEXES:
        default: {
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return;
        }
    }

    if(rebuild) {
        GLuint updateMask = (pname == GL_AMBIENT) ? AMBIENT_MASK:
                            (pname == GL_DIFFUSE) ? DIFFUSE_MASK:
                            (pname == GL_SPECULAR) ? SPECULAR_MASK:
                            (pname == GL_EMISSION) ? EMISSION_MASK:
                            (pname == GL_AMBIENT_AND_DIFFUSE) ? AMBIENT_MASK | DIFFUSE_MASK : 0;

        _glPrecalcLightingValues(updateMask);
    }
}

void APIENTRY glColorMaterial(GLenum face, GLenum mode) {
    if(face != GL_FRONT_AND_BACK) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    GLint validModes[] = {GL_AMBIENT, GL_DIFFUSE, GL_AMBIENT_AND_DIFFUSE, GL_EMISSION, GL_SPECULAR, 0};

    if(_glCheckValidEnum(mode, validModes, __func__) != 0) {
        return;
    }

    GLenum mask = (mode == GL_AMBIENT) ? AMBIENT_MASK:
                          (mode == GL_DIFFUSE) ? DIFFUSE_MASK:
                          (mode == GL_AMBIENT_AND_DIFFUSE) ? AMBIENT_MASK | DIFFUSE_MASK:
                          (mode == GL_EMISSION) ? EMISSION_MASK : SPECULAR_MASK;

    _glSetColorMaterialMask(mask);
    _glSetColorMaterialMode(mode);
}

GL_FORCE_INLINE void bgra_to_float(const uint8_t* input, GLfloat* output) {
    static const float scale = 1.0f / 255.0f;

    output[0] = ((float) input[R8IDX]) * scale;
    output[1] = ((float) input[G8IDX]) * scale;
    output[2] = ((float) input[B8IDX]) * scale;
    output[3] = ((float) input[A8IDX]) * scale;
}

void _glUpdateColourMaterialA(const GLubyte* argb) {
    Material* material = _glActiveMaterial();

    float colour[4];
    bgra_to_float(argb, colour);
    vec4cpy(material->ambient, colour);
    GLenum mask = _glColorMaterialMode();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialD(const GLubyte* argb) {
    Material* material = _glActiveMaterial();

    float colour[4];
    bgra_to_float(argb, colour);
    vec4cpy(material->diffuse, colour);

    GLenum mask = _glColorMaterialMode();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialE(const GLubyte* argb) {
    Material* material = _glActiveMaterial();

    float colour[4];
    bgra_to_float(argb, colour);
    vec4cpy(material->emissive, colour);

    GLenum mask = _glColorMaterialMode();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialAD(const GLubyte* argb) {
    Material* material = _glActiveMaterial();

    float colour[4];
    bgra_to_float(argb, colour);
    vec4cpy(material->ambient, colour);
    vec4cpy(material->diffuse, colour);

    GLenum mask = _glColorMaterialMode();
    _glPrecalcLightingValues(mask);
}

GL_FORCE_INLINE GLboolean isDiffuseColorMaterial() {
    GLenum mode = _glColorMaterialMode();
    return (
        mode == GL_DIFFUSE ||
        mode == GL_AMBIENT_AND_DIFFUSE
    );
}

GL_FORCE_INLINE GLboolean isAmbientColorMaterial() {
    GLenum mode = _glColorMaterialMode();
    return (
        mode == GL_AMBIENT ||
        mode == GL_AMBIENT_AND_DIFFUSE
    );
}

GL_FORCE_INLINE GLboolean isSpecularColorMaterial() {
    GLenum mode = _glColorMaterialMode();
    return (mode == GL_SPECULAR);
}

/*
 * Implementation from here (MIT):
 * https://github.com/appleseedhq/appleseed/blob/master/src/appleseed/foundation/math/fastmath.h
 */
GL_FORCE_INLINE float faster_pow2(const float p) {
    // Underflow of exponential is common practice in numerical routines, so handle it here.
    const float clipp = p < -126.0f ? -126.0f : p;
    const union { uint32_t i; float f; } v =
    {
        (uint32_t) ((1 << 23) * (clipp + 126.94269504f))
    };

    return v.f;
}

GL_FORCE_INLINE float faster_log2(const float x) {
    gl_assert(x >= 0.0f);

    const union { float f; uint32_t i; } vx = { x };
    const float y = (float) (vx.i) * 1.1920928955078125e-7f;

    return y - 126.94269504f;
}

GL_FORCE_INLINE float faster_pow(const float x, const float p) {
    return faster_pow2(p * faster_log2(x));
}

GL_FORCE_INLINE void _glLightVertexDirectional(
    float* final, uint8_t lid,
    float LdotN, float NdotH) {

    Material* material = _glActiveMaterial();
    LightSource* light = _glLightAt(lid);

    float FI = (material->exponent) ?
        faster_pow((LdotN != 0.0f) * NdotH, material->exponent) : 1.0f;

#define _PROCESS_COMPONENT(X) \
    final[X] += (LdotN * light->diffuseMaterial[X] + light->ambientMaterial[X]) \
        + (FI * light->specularMaterial[X]); \

    _PROCESS_COMPONENT(0);
    _PROCESS_COMPONENT(1);
    _PROCESS_COMPONENT(2);

#undef _PROCESS_COMPONENT
}

GL_FORCE_INLINE void _glLightVertexPoint(
    float* final, uint8_t lid,
    float LdotN, float NdotH, float att) {

    Material* material = _glActiveMaterial();
    LightSource* light = _glLightAt(lid);

    float FI = (material->exponent) ?
        faster_pow((LdotN != 0.0f) * NdotH, material->exponent) : 1.0f;

#define _PROCESS_COMPONENT(X) \
    final[X] += ((LdotN * light->diffuseMaterial[X] + light->ambientMaterial[X]) \
        + (FI * light->specularMaterial[X])) * att; \

    _PROCESS_COMPONENT(0);
    _PROCESS_COMPONENT(1);
    _PROCESS_COMPONENT(2);

#undef _PROCESS_COMPONENT
}

void _glPerformLighting(Vertex* vertices, EyeSpaceData* es, const uint32_t count) {
    GLubyte i;
    GLuint j;

    Material* material = _glActiveMaterial();

    Vertex* vertex = vertices;
    EyeSpaceData* data = es;

    /* Calculate the colour material function once */
    void (*updateColourMaterial)(const GLubyte*) = NULL;

    if(_glIsColorMaterialEnabled()) {
        GLenum mode = _glColorMaterialMode();
        switch(mode) {
            case GL_AMBIENT:
                updateColourMaterial = _glUpdateColourMaterialA;
            break;
            case GL_DIFFUSE:
                updateColourMaterial = _glUpdateColourMaterialD;
            break;
            case GL_EMISSION:
                updateColourMaterial = _glUpdateColourMaterialE;
            break;
            case GL_AMBIENT_AND_DIFFUSE:
                updateColourMaterial = _glUpdateColourMaterialAD;
            break;
        }
    }

    /* Calculate the ambient lighting and set up colour material */
    for(j = 0; j < count; ++j, ++vertex, ++data) {
        if(updateColourMaterial) {
            updateColourMaterial(vertex->bgra);
        }

        /* Copy the base colour across */
        vec4cpy(data->finalColour, material->baseColour);
    }

    if(!_glEnabledLightCount()) {
        return;
    }

    vertex = vertices;
    data = es;
    for(j = 0; j < count; ++j, ++vertex, ++data) {
        /* Direction to vertex in eye space */
        float Vx = -vertex->xyz[0];
        float Vy = -vertex->xyz[1];
        float Vz = -vertex->xyz[2];
        VEC3_NORMALIZE(Vx, Vy, Vz);

        const float Nx = data->n[0];
        const float Ny = data->n[1];
        const float Nz = data->n[2];

        for(i = 0; i < MAX_GLDC_LIGHTS; ++i) {
            LightSource* light = _glLightAt(i);

            if(!light->isEnabled) {
                continue;
            }

            float Lx = light->position[0] - vertex->xyz[0];
            float Ly = light->position[1] - vertex->xyz[1];
            float Lz = light->position[2] - vertex->xyz[2];

            if(light->isDirectional) {
                float Hx = (Lx + 0);
                float Hy = (Ly + 0);
                float Hz = (Lz + 1);

                VEC3_NORMALIZE(Lx, Ly, Lz);
                VEC3_NORMALIZE(Hx, Hy, Hz);

                float LdotN, NdotH;
                VEC3_DOT(
                    Nx, Ny, Nz, Lx, Ly, Lz, LdotN
                );

                VEC3_DOT(
                    Nx, Ny, Nz, Hx, Hy, Hz, NdotH
                );

                if(LdotN < 0.0f) LdotN = 0.0f;
                if(NdotH < 0.0f) NdotH = 0.0f;

                _glLightVertexDirectional(
                    data->finalColour,
                    i, LdotN, NdotH
                );
            } else {
                float D;
                VEC3_LENGTH(Lx, Ly, Lz, D);

                float att = (
                    light->constant_attenuation + (
                        light->linear_attenuation * D
                    ) + (light->quadratic_attenuation * D * D)
                );

                /* Anything over the attenuation threshold will
                 * be a tiny value after inversion (< 0.01f) so
                 * let's just skip the lighting at that point */
                if(att < ATTENUATION_THRESHOLD) {
                    att = MATH_Fast_Invert(att);

                    float Hx = (Lx + Vx);
                    float Hy = (Ly + Vy);
                    float Hz = (Lz + Vz);

                    VEC3_NORMALIZE(Lx, Ly, Lz);
                    VEC3_NORMALIZE(Hx, Hy, Hz);

                    float LdotN, NdotH;
                    VEC3_DOT(
                        Nx, Ny, Nz, Lx, Ly, Lz, LdotN
                    );

                    VEC3_DOT(
                        Nx, Ny, Nz, Hx, Hy, Hz, NdotH
                    );

                    if(LdotN < 0.0f) LdotN = 0.0f;
                    if(NdotH < 0.0f) NdotH = 0.0f;

                    _glLightVertexPoint(
                        data->finalColour,
                        i, LdotN, NdotH, att
                    );
                }
            }
        }

        vertex->bgra[R8IDX] = clamp(data->finalColour[0] * 255.0f, 0, 255);
        vertex->bgra[G8IDX] = clamp(data->finalColour[1] * 255.0f, 0, 255);
        vertex->bgra[B8IDX] = clamp(data->finalColour[2] * 255.0f, 0, 255);
        vertex->bgra[A8IDX] = clamp(data->finalColour[3] * 255.0f, 0, 255);
    }
}

#undef LIGHT_COMPONENT
