#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <dc/vec3f.h>
#include "private.h"

#define _MIN(x, y) (x < y) ? x : y

/* Lighting will not be calculated if the attenuation
 * multiplier ends up less than this value */
#define ATTENUATION_THRESHOLD 0.01f

static GLfloat SCENE_AMBIENT [] = {0.2f, 0.2f, 0.2f, 1.0f};
static GLboolean VIEWER_IN_EYE_COORDINATES = GL_TRUE;
static GLenum COLOR_CONTROL = GL_SINGLE_COLOR;

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

            if(params[3] == 0.0f) {
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

    MATERIAL.exponent = _MIN(param, 128);  /* 128 is the max according to the GL spec */
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
            memcpy(MATERIAL.emissive, params, sizeof(GLfloat) * 4);
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

GL_FORCE_INLINE GLboolean isDiffuseColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_DIFFUSE || COLOR_MATERIAL_MODE == GL_AMBIENT_AND_DIFFUSE);
}

GL_FORCE_INLINE GLboolean isAmbientColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_AMBIENT || COLOR_MATERIAL_MODE == GL_AMBIENT_AND_DIFFUSE);
}

GL_FORCE_INLINE GLboolean isSpecularColorMaterial() {
    return (COLOR_MATERIAL_MODE == GL_SPECULAR);
}

GL_FORCE_INLINE void initVec3(struct vec3f* v, const GLfloat* src) {
    memcpy(v, src, sizeof(GLfloat) * 3);
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
    assert(x >= 0.0f);

    const union { float f; uint32_t i; } vx = { x };
    const float y = (float) (vx.i) * 1.1920928955078125e-7f;

    return y - 126.94269504f;
}

GL_FORCE_INLINE float faster_pow(const float x, const float p) {
    return faster_pow2(p * faster_log2(x));
}

GL_FORCE_INLINE float vec3_dot_limited(
        const float* x1, const float* y1, const float* z1,
        const float* x2, const float* y2, const float* z2) {

    float ret;
    vec3f_dot(*x1, *y1, *z1, *x2, *y2, *z2, ret);
    return (ret < 0) ? 0 : ret;
}

GL_FORCE_INLINE void _glLightVertexDirectional(
    uint8_t* final, int8_t lid,
    float LdotN, float NdotH,
    const float* ambient, const float* diffuse, const float* specular) {

    float F;
    uint8_t FO;
    float FI = (LdotN != 0.0f);
    FI = (MATERIAL.exponent) ? faster_pow(FI * NdotH, MATERIAL.exponent) : 1.0f;

#define _PROCESS_COMPONENT(T, X) \
    F = (ambient[X] * LIGHTS[lid].ambient[X]); \
    F += (LdotN * diffuse[X] * LIGHTS[lid].diffuse[X]); \
    F += FI * specular[X] * LIGHTS[lid].specular[X]; \
    FO = (uint8_t) (_MIN(F * 255.0f, 255.0f)); \
    final[T] += _MIN(FO, 255 - final[T]);

    _PROCESS_COMPONENT(R8IDX, 0);
    _PROCESS_COMPONENT(G8IDX, 1);
    _PROCESS_COMPONENT(B8IDX, 2);

#undef _PROCESS_COMPONENT
}

GL_FORCE_INLINE void _glLightVertexPoint(
    uint8_t* final, int8_t lid,
    float LdotN, float NdotH, float att,
    const float* ambient, const float* diffuse, const float* specular) {

    float F;
    uint8_t FO;
    float FI = (LdotN != 0.0f);
    FI = (MATERIAL.exponent) ? faster_pow(FI * NdotH, MATERIAL.exponent) : 1.0f;

#define _PROCESS_COMPONENT(T, X) \
    F = (ambient[X] * LIGHTS[lid].ambient[X]); \
    F += (LdotN * diffuse[X] * LIGHTS[lid].diffuse[X]); \
    F += FI * specular[X] * LIGHTS[lid].specular[X]; \
    FO = (uint8_t) (_MIN(F * att * 255.0f, 255.0f)); \
\
    final[T] += _MIN(FO, 255 - final[T]); \

    _PROCESS_COMPONENT(R8IDX, 0);
    _PROCESS_COMPONENT(G8IDX, 1);
    _PROCESS_COMPONENT(B8IDX, 2);

#undef _PROCESS_COMPONENT
}

GL_FORCE_INLINE void bgra_to_float(const uint8_t* input, GLfloat* output) {
    static const float scale = 1.0f / 255.0f;

    output[0] = ((float) input[R8IDX]) * scale;
    output[1] = ((float) input[G8IDX]) * scale;
    output[2] = ((float) input[B8IDX]) * scale;
    output[3] = ((float) input[A8IDX]) * scale;
}

void _glPerformLighting(Vertex* vertices, const EyeSpaceData* es, const int32_t count) {
    int8_t i;
    int32_t j;

    Vertex* vertex = vertices;
    const EyeSpaceData* data = es;
    float base;

    /* This is the original vertex colour, before we replace it. It's
     * used for colour material */
    float vdiffuse[4];

    unsigned char isCM = _glIsColorMaterialEnabled();

    /* Update pointers as necessary depending on color material */
    GLfloat* ambient = (isCM && isAmbientColorMaterial()) ? vdiffuse : MATERIAL.ambient;
    GLfloat* diffuse = (isCM && isDiffuseColorMaterial()) ? vdiffuse : MATERIAL.diffuse;
    GLfloat* specular = (isCM && isSpecularColorMaterial()) ? vdiffuse : MATERIAL.specular;

    for(j = 0; j < count; ++j, ++vertex, ++data) {
        /* Unpack the colour for use in glColorMaterial */
        if(isCM) {
            bgra_to_float(vertex->bgra, vdiffuse);
        }

        /* Initial, non-light related values */
        base = (SCENE_AMBIENT[0] * ambient[0]) + MATERIAL.emissive[0];
        vertex->bgra[R8IDX] = (uint8_t)(_MIN(base * 255.0f, 255.0f));

        base = (SCENE_AMBIENT[1] * ambient[1]) + MATERIAL.emissive[1];
        vertex->bgra[G8IDX] = (uint8_t)(_MIN(base * 255.0f, 255.0f));

        base = (SCENE_AMBIENT[2] * ambient[2]) + MATERIAL.emissive[2];
        vertex->bgra[B8IDX] = (uint8_t)(_MIN(base * 255.0f, 255.0f));
        vertex->bgra[A8IDX] = (uint8_t)(_MIN(MATERIAL.diffuse[3] * 255.0f, 255.0f));

        /* Direction to vertex in eye space */
        float Vx = -data->xyz[0];
        float Vy = -data->xyz[1];
        float Vz = -data->xyz[2];
        vec3f_normalize(Vx, Vy, Vz);

        const float Nx = data->n[0];
        const float Ny = data->n[1];
        const float Nz = data->n[2];

        for(i = 0; i < MAX_LIGHTS; ++i) {
            if(!_glIsLightEnabled(i)) continue;

            if(LIGHTS[i].position[3] == 0.0f) {
                float Lx = LIGHTS[i].position[0] - data->xyz[0];
                float Ly = LIGHTS[i].position[1] - data->xyz[1];
                float Lz = LIGHTS[i].position[2] - data->xyz[2];

                float Hx = (Lx + 0);
                float Hy = (Ly + 0);
                float Hz = (Lz + 1);

                vec3f_normalize(Lx, Ly, Lz);
                vec3f_normalize(Hx, Hy, Hz);

                const float LdotN = vec3_dot_limited(
                    &Nx, &Ny, &Nz,
                    &Lx, &Ly, &Lz
                );

                const float NdotH = vec3_dot_limited(
                    &Nx, &Ny, &Nz,
                    &Hx, &Hy, &Hz
                );

                _glLightVertexDirectional(
                    vertex->bgra,
                    i, LdotN, NdotH,
                    ambient, diffuse, specular
                );
            } else {
                float Lx = LIGHTS[i].position[0] - data->xyz[0];
                float Ly = LIGHTS[i].position[1] - data->xyz[1];
                float Lz = LIGHTS[i].position[2] - data->xyz[2];
                float D;

                vec3f_length(Lx, Ly, Lz, D);

                float att = (
                    LIGHTS[i].constant_attenuation + (
                        LIGHTS[i].linear_attenuation * D
                    ) + (LIGHTS[i].quadratic_attenuation * D * D)
                );

                att = MATH_fsrra(att * att);

                if(att >= ATTENUATION_THRESHOLD) {
                    float Hx = (Lx + Vx);
                    float Hy = (Ly + Vy);
                    float Hz = (Lz + Vz);

                    vec3f_normalize(Lx, Ly, Lz);
                    vec3f_normalize(Hx, Hy, Hz);

                    const float LdotN = vec3_dot_limited(
                        &Nx, &Ny, &Nz,
                        &Lx, &Ly, &Lz
                    );

                    const float NdotH = vec3_dot_limited(
                        &Nx, &Ny, &Nz,
                        &Hx, &Hy, &Hz
                    );

                    _glLightVertexPoint(
                        vertex->bgra,
                        i, LdotN, NdotH, att,
                        ambient, diffuse, specular
                    );
                }
            }
        }
    }
}

#undef LIGHT_COMPONENT
