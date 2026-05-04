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

/* Fast normal unpacking constants: 1/127.5 and -1.0 */
#define NORMAL_SCALE 0.00784313725f
#define NORMAL_OFFSET -1.0f

/* Maximum light range for early-out optimization */
#define MAX_LIGHT_RANGE 10.0f

/* PI constant for spotlight calculations */
#define GL_PI 3.14159265358979323846f


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

        material->baseColour[0] = scene_ambient[0] * material->ambient[0] + material->emissive[0];
        material->baseColour[1] = scene_ambient[1] * material->ambient[1] + material->emissive[1];
        material->baseColour[2] = scene_ambient[2] * material->ambient[2] + material->emissive[2];
        material->baseColour[3] = scene_ambient[3] * material->ambient[3] + material->emissive[3];
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
        light->spot_cutoff_cos = -1.0f;  /* cos(180°) = -1.0 */

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
        case GL_SPOT_CUTOFF: {
            /* Validate spot_cutoff per GL spec: [0, 90] or 180 */
            if(param >= 0.0f && param <= 90.0f) {
                l->spot_cutoff = param;
                l->spot_cutoff_cos = cosf(param * GL_PI / 180.0f);
            } else if(param == 180.0f) {
                l->spot_cutoff = 180.0f;
                l->spot_cutoff_cos = -1.0f;
            } else {
                _glKosThrowError(GL_INVALID_VALUE, __func__);
                return;
            }
        }
        break;
        case GL_SPOT_DIRECTION:
            /* spot_direction is assumed to be in eye space */
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

void _glUpdateColourMaterialA(const float* colour) {
    Material* material = _glActiveMaterial();
    vec4cpy(material->ambient, colour);
    GLenum mask = _glColorMaterialMask();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialD(const float* colour) {
    Material* material = _glActiveMaterial();

    vec4cpy(material->diffuse, colour);

    GLenum mask = _glColorMaterialMask();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialE(const float* colour) {
    Material* material = _glActiveMaterial();
    vec4cpy(material->emissive, colour);

    GLenum mask = _glColorMaterialMask();
    _glPrecalcLightingValues(mask);
}

void _glUpdateColourMaterialAD(const float* colour) {
    Material* material = _glActiveMaterial();

    vec4cpy(material->ambient, colour);
    vec4cpy(material->diffuse, colour);

    GLenum mask = _glColorMaterialMask();
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

/* Compute the specular term based on NdotH and material shininess */
GL_FORCE_INLINE float computeSpecular(float NdotH, GLfloat exponent) {
    if(exponent > 0.0f) {
        return faster_pow(NdotH, exponent);
    }
    /* When shininess is 0, specular is 1.0 if NdotH > 0, otherwise 0.0 */
    return (NdotH > 0.0f) ? 1.0f : 0.0f;
}

/* Apply lighting contribution to the final colour */
#define _PROCESS_LIGHTING_COMPONENT(final, X, LdotN, NdotH, FI, light, isPoint, att) \
    do { \
        float diffuseAmbient = LdotN * (light)->diffuseMaterial[X] + (light)->ambientMaterial[X]; \
        float specular = FI * (light)->specularMaterial[X]; \
        if(isPoint) { \
            (final)[X] += (diffuseAmbient + specular) * (att); \
        } else { \
            (final)[X] += diffuseAmbient + specular; \
        } \
    } while(0)

/* Process directional light contribution */
GL_FORCE_INLINE void accumulateDirectionalLight(
    float* finalColour,
    LightSource* light,
    float LdotN,
    float NdotH,
    GLfloat exponent
) {
    float FI = computeSpecular(NdotH, exponent);

    _PROCESS_LIGHTING_COMPONENT(finalColour, 0, LdotN, NdotH, FI, light, 0, 0);
    _PROCESS_LIGHTING_COMPONENT(finalColour, 1, LdotN, NdotH, FI, light, 0, 0);
    _PROCESS_LIGHTING_COMPONENT(finalColour, 2, LdotN, NdotH, FI, light, 0, 0);
}

/* Process point/spot light contribution */
GL_FORCE_INLINE void accumulatePointLight(
    float* finalColour,
    LightSource* light,
    float LdotN,
    float NdotH,
    GLfloat exponent,
    float attenuation
) {
    float FI = computeSpecular(NdotH, exponent);

    _PROCESS_LIGHTING_COMPONENT(finalColour, 0, LdotN, NdotH, FI, light, 1, attenuation);
    _PROCESS_LIGHTING_COMPONENT(finalColour, 1, LdotN, NdotH, FI, light, 1, attenuation);
    _PROCESS_LIGHTING_COMPONENT(finalColour, 2, LdotN, NdotH, FI, light, 1, attenuation);
}

#undef _PROCESS_LIGHTING_COMPONENT

/* Compute spotlight factor based on angle between light direction and spot direction.
 * Returns 1.0 for non-spotlights (spot_cutoff == 180). */
GL_FORCE_INLINE float computeSpotFactor(
    const LightSource* light,
    float Lx, float Ly, float Lz
) {
    /* Not a spotlight if cutoff is 180 (full sphere) */
    if(light->spot_cutoff >= 179.0f) {
        return 1.0f;
    }

    /* Compute -L · spot_direction (L points FROM vertex TO light,
     * spot_direction points FROM light outward, so we negate L) */
    float spotDot = -(
        Lx * light->spot_direction[0] +
        Ly * light->spot_direction[1] +
        Lz * light->spot_direction[2]
    );

    /* GL spec: spot is active when -L·spot_direction >= cos(spot_cutoff) */
    if(spotDot < light->spot_cutoff_cos) {
        return 0.0f;
    }

    /* spot_factor = (-L · spot_direction)^spot_exponent */
    if(light->spot_exponent > 0.0f) {
        return faster_pow(spotDot, light->spot_exponent);
    }

    return 1.0f;
}

/* Unpack normal from packed 24-bit format (8-bit X, Y, Z components) */
GL_FORCE_INLINE void unpackNormal(uint32_t packed, float* outNx, float* outNy, float* outNz) {
    *outNx = ((packed >> 16) & 0xFF) * NORMAL_SCALE + NORMAL_OFFSET;
    *outNy = ((packed >> 8) & 0xFF) * NORMAL_SCALE + NORMAL_OFFSET;
    *outNz = (packed & 0xFF) * NORMAL_SCALE + NORMAL_OFFSET;
}

/* Compute view vector based on LOCAL_VIEWER setting */
GL_FORCE_INLINE void computeViewVector(
    const Vertex* vertex,
    GLboolean localViewer,
    float* outVx, float* outVy, float* outVz
) {
    if(localViewer) {
        /* Local viewer: V = -vertex position (normalized) */
        *outVx = -vertex->xyz[0];
        *outVy = -vertex->xyz[1];
        *outVz = -vertex->xyz[2];
        VEC3_NORMALIZE(*outVx, *outVy, *outVz);
    } else {
        /* Infinite viewer: V = (0, 0, 1) - looking down -Z axis */
        *outVx = 0.0f;
        *outVy = 0.0f;
        *outVz = 1.0f;
    }
}

/* Compute light direction vector L from vertex to light source.
 * Returns 1 if directional (no normalization needed), 0 if point/spot. */
GL_FORCE_INLINE int computeLightVector(
    const LightSource* light,
    const Vertex* vertex,
    float* outLx, float* outLy, float* outLz
) {
    if(light->isDirectional) {
        /* Directional lights: position is a direction vector (w=0).
         * L = -light->position (direction from vertex to light at infinity) */
        *outLx = -light->position[0];
        *outLy = -light->position[1];
        *outLz = -light->position[2];
        
        /* Ensure normalized (should already be if set up correctly) */
        float lenSq = (*outLx)*(*outLx) + (*outLy)*(*outLy) + (*outLz)*(*outLz);
        if(lenSq > 0.0f && lenSq != 1.0f) {
            float invLen = MATH_fsrra(lenSq);
            *outLx *= invLen;
            *outLy *= invLen;
            *outLz *= invLen;
        }
        return 1; /* Directional */
    } else {
        /* Point/spot light: L = light position - vertex position */
        *outLx = light->position[0] - vertex->xyz[0];
        *outLy = light->position[1] - vertex->xyz[1];
        *outLz = light->position[2] - vertex->xyz[2];
        return 0; /* Point/spot */
    }
}

/* Process a single vertex through the lighting pipeline */
GL_FORCE_INLINE void _glProcessVertex(
    Vertex* vertex,
    float* finalColour,
    const Material* material,
    LightSource** enabledLights,
    const GLuint enabledCount,
    void (*colorMaterialFunc)(const float*),
    GLboolean localViewer
) {
    /* Update color material if function provided */
    if(colorMaterialFunc) {
        colorMaterialFunc(vertex->argb);
    }

    /* Prefetch next vertex while processing current */
#ifdef _arch_dreamcast
    PREFETCH(vertex + 1);
#endif

    /* Unpack normal */
    float Nx, Ny, Nz;
    unpackNormal(vertex->nxyz, &Nx, &Ny, &Nz);

    /* Compute view vector */
    float Vx, Vy, Vz;
    computeViewVector(vertex, localViewer, &Vx, &Vy, &Vz);

    /* Copy base colour */
    vec4cpy(finalColour, material->baseColour);

    const GLfloat exponent = material->exponent;

    /* Light loop */
    for(GLubyte li = 0; li < enabledCount; ++li) {
        LightSource* light = enabledLights[li];

        /* Compute light direction */
        float Lx, Ly, Lz;
        int isDirectional = computeLightVector(light, vertex, &Lx, &Ly, &Lz);

        if(isDirectional) {
            /* Directional light: no attenuation */
            /* Half-vector: H = (L + V) / |L + V| */
            float Hx = Lx + Vx;
            float Hy = Ly + Vy;
            float Hz = Lz + Vz;
            VEC3_NORMALIZE(Hx, Hy, Hz);

            float LdotN, NdotH;
            VEC3_DOT(Nx, Ny, Nz, Lx, Ly, Lz, LdotN);
            VEC3_DOT(Nx, Ny, Nz, Hx, Hy, Hz, NdotH);

            /* Clamp to zero */
            if(LdotN < 0.0f) LdotN = 0.0f;
            if(NdotH < 0.0f) NdotH = 0.0f;

            accumulateDirectionalLight(finalColour, light, LdotN, NdotH, exponent);
        } else {
            /* Point/spot light: compute distance */
            float D;
            VEC3_LENGTH(Lx, Ly, Lz, D);

            /* Early-out: skip distant lights */
            if(D > MAX_LIGHT_RANGE) {
                continue;
            }

            /* Compute spotlight factor */
            float spotFactor = computeSpotFactor(light, Lx, Ly, Lz);
            if(spotFactor <= 0.0f) {
                continue;
            }

            /* Compute combined attenuation with spotlight */
            float att = light->constant_attenuation +
                       light->linear_attenuation * D +
                       light->quadratic_attenuation * D * D;
            float combinedAtt = att / spotFactor;

            if(combinedAtt < ATTENUATION_THRESHOLD) {
                combinedAtt = MATH_Fast_Invert(combinedAtt);

                /* Normalize L for dot products */
                VEC3_NORMALIZE(Lx, Ly, Lz);

                /* Half-vector: H = (L + V) / |L + V| */
                float Hx = Lx + Vx;
                float Hy = Ly + Vy;
                float Hz = Lz + Vz;
                VEC3_NORMALIZE(Hx, Hy, Hz);

                float LdotN, NdotH;
                VEC3_DOT(Nx, Ny, Nz, Lx, Ly, Lz, LdotN);
                VEC3_DOT(Nx, Ny, Nz, Hx, Hy, Hz, NdotH);

                /* Clamp to zero */
                if(LdotN < 0.0f) LdotN = 0.0f;
                if(NdotH < 0.0f) NdotH = 0.0f;

                accumulatePointLight(finalColour, light, LdotN, NdotH, exponent, combinedAtt);
            }
        }
    }

    /* Write final colour */
    vertex->argb[R8IDX] = finalColour[0];
    vertex->argb[G8IDX] = finalColour[1];
    vertex->argb[B8IDX] = finalColour[2];
    vertex->argb[A8IDX] = finalColour[3];
}

void _glPerformLighting(Vertex* vertices, const uint32_t count) {
    if(!_glEnabledLightCount()) {
        return;
    }

    const Material* material = _glActiveMaterial();
    LightSource** enabledLights = _glEnabledLightCache();
    const GLuint enabledCount = _glEnabledLightCount();

    /* Reuse finalColour outside the vertex loop */
    float finalColour[4];

    /* Read LOCAL_VIEWER setting once */
    GLboolean localViewer = _glGetLightModelViewerInEyeCoordinates();

    /* Select the appropriate color material function */
    void (*colorMaterialFunc)(const float*) = NULL;
    if(_glIsColorMaterialEnabled()) {
        GLenum mode = _glColorMaterialMode();
        switch(mode) {
            case GL_AMBIENT:
                colorMaterialFunc = _glUpdateColourMaterialA;
            break;
            case GL_DIFFUSE:
                colorMaterialFunc = _glUpdateColourMaterialD;
            break;
            case GL_EMISSION:
                colorMaterialFunc = _glUpdateColourMaterialE;
            break;
            case GL_AMBIENT_AND_DIFFUSE:
                colorMaterialFunc = _glUpdateColourMaterialAD;
            break;
            default:
                /* No color material update for specular or other modes */
            break;
        }
    }

    /* Process all vertices */
    Vertex* vertex = vertices;
    for(uint32_t j = 0; j < count; ++j, ++vertex) {
        _glProcessVertex(
            vertex,
            finalColour,
            material,
            enabledLights,
            enabledCount,
            colorMaterialFunc,
            localViewer
        );
    }
}

#undef LIGHT_COMPONENT
