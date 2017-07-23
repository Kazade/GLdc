/* KallistiGL for KallistiOS ##version##

   libgl/gl-light.c
   Copyright (C) 2013-2014 Josh Pearson

   Dynamic Vertex Lighting Model:
   vertexColor = emissive + ambient + ( diffuse + specular * attenuation )

   The only difference here from real OpenGL is that only 1 ambient light
   source is used, as opposed to each light containing its own abmient value.
   Abmient light is set by the glKosLightAbmient..(..) functions below.

   By default, the specular lighting term is enabled.
   For now, specular can be disabled by setting GL_ENABLE_SPECULAR on
   gl-light.h when you build the library.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gl.h"
#include "gl-api.h"
#include "gl-clip.h"
#include "gl-light.h"

#define GL_KOS_MAX_LIGHTS 16 /* Number of Light Sources that may be enabled at once */

static GLfloat GL_GLOBAL_AMBIENT[4] = { 0, 0, 0, 0 }; /* RGBA Global Ambient Light */
static GLfloat GL_VERTEX_NORMAL[3] = { 0, 0, 0 };     /* Current Vertex Normal */
static GLbitfield GL_LIGHT_ENABLED = 0;               /* Client State for Enabling Lighting */

static glLight GL_LIGHTS[GL_KOS_MAX_LIGHTS],
GL_DEFAULT_LIGHT = { { 0.0, 0.0, 1.0, 0.0 },   /* Position */
    { 0.0, 0.0, -1.0 },       /* Spot Direction */
    -1.0f,                    /* Spot Cutoff */
    1.0f, 0.0f, 0.0f,         /* Attenuation Factors */
    0.0f,                     /* Spot Exponent */
    { 1.0, 1.0, 1.0, 1.0 },   /* Diffuse */
    { 1.0, 1.0, 1.0, 1.0 },   /* Specular */
    { 0.0, 0.0, 0.0, 1.0 }    /* Ambient */
};

static glMaterial GL_MATERIAL,
GL_DEFAULT_MATERIAL = { { 0.0, 0.0, 0.0, 1.0 }, /* Emissive Color */
    { 0.2, 0.2, 0.2, 1.0 }, /* Ambient Reflectance */
    { 0.8, 0.8, 0.8, 1.0 }, /* Diffuse Reflectance */
    { 0.0, 0.0, 0.0, 1.0 }, /* Specular Reflectance */
    0.0                     /* Shininess */
};

static GLfloat GL_EYE_POSITION[3] = { 0, 0, 0 }; /* Eye Position for Specular Factor */

void _glKosSetEyePosition(GLfloat *position) {  /* Called internally by glhLookAtf() */
    GL_EYE_POSITION[0] = position[0];
    GL_EYE_POSITION[1] = position[1];
    GL_EYE_POSITION[2] = position[2];
}

void _glKosInitLighting() { /* Called internally by glInit() */
    unsigned char i;

    for(i = 0; i < GL_KOS_MAX_LIGHTS; i++)
        memcpy(&GL_LIGHTS[i], &GL_DEFAULT_LIGHT, sizeof(glLight));

    memcpy(&GL_MATERIAL, &GL_DEFAULT_MATERIAL, sizeof(glMaterial));
}

/* Enable a light - GL_LIGHT0->GL_LIGHT7 */
void _glKosEnableLight(const GLuint light) {
    if(light < GL_LIGHT0 || light > GL_LIGHT0 + GL_KOS_MAX_LIGHTS) {
        _glKosThrowError(GL_INVALID_ENUM, "glEnable(GL_LIGHT)");
        return;
    }

    GL_LIGHT_ENABLED |= (1 << (light & 0xF));
}

/* Disable a light - GL_LIGHT0->GL_LIGHT0 + GL_KOS_MAX_LIGHTS */
void _glKosDisableLight(const GLuint light) {
    if(light < GL_LIGHT0 || light > GL_LIGHT0 + GL_KOS_MAX_LIGHTS) {
        _glKosThrowError(GL_INVALID_ENUM, "glDisable(GL_LIGHT)");
        return;
    }

    GL_LIGHT_ENABLED &= ~(1 << (light & 0xF));
}

GLubyte _glKosIsLightEnabled(GLubyte light) {
    return GL_LIGHT_ENABLED & (1 << light);
}

GLubyte _glKosGetMaxLights() {
    return GL_KOS_MAX_LIGHTS;
}

/* Vertex Normal Submission */
void glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    GL_VERTEX_NORMAL[0] = x;
    GL_VERTEX_NORMAL[1] = y;
    GL_VERTEX_NORMAL[2] = z;
}

void glNormal3fv(const GLfloat *xyz) {
    GL_VERTEX_NORMAL[0] = xyz[0];
    GL_VERTEX_NORMAL[1] = xyz[1];
    GL_VERTEX_NORMAL[2] = xyz[2];
}

void glLightModelfv(GLenum pname, const GLfloat *params) {
    if(pname != GL_LIGHT_MODEL_AMBIENT) {
        _glKosThrowError(GL_INVALID_ENUM, "glLightModelfv");
        return;
    }

    GL_GLOBAL_AMBIENT[0] = params[0];
    GL_GLOBAL_AMBIENT[1] = params[1];
    GL_GLOBAL_AMBIENT[2] = params[2];
    GL_GLOBAL_AMBIENT[3] = params[3];
}

/* Misc Lighting Functions ************************************/
static inline void glCopyRGBA(const rgba *src, rgba *dst) {
    *dst = *src;
}

static inline void glCopy4f(const float *src, float *dst) {
    memcpy(dst, src, 0x10);
}

static inline void glCopy3f(const float *src, float *dst) {
    memcpy(dst, src, 0xC);
}

/* GL Light Parameters ******************************************************/

void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    if(light < GL_LIGHT0 || light > GL_LIGHT0 + GL_KOS_MAX_LIGHTS) return;

    switch(pname) {
        case GL_AMBIENT:
            glCopyRGBA((rgba *)params, (rgba *)&GL_LIGHTS[light & 0xF].Ka);
            break;

        case GL_DIFFUSE:
            glCopyRGBA((rgba *)params, (rgba *)&GL_LIGHTS[light & 0xF].Kd);
            break;

        case GL_SPECULAR:
            glCopyRGBA((rgba *)params, (rgba *)&GL_LIGHTS[light & 0xF].Ks);
            break;

        case GL_POSITION:
            glCopy4f(params, &GL_LIGHTS[light & 0xF].Pos[0]);
            break;

        case GL_SPOT_DIRECTION:
            glCopy3f(params, &GL_LIGHTS[light & 0xF].Dir[0]);
            break;

        case GL_SPOT_EXPONENT:
            GL_LIGHTS[light & 0xF].Exponent = *params;
            break;

        case GL_SPOT_CUTOFF:
            if(*params >= 0.0f && *params <= 180.0f)
                GL_LIGHTS[light & 0xF].CutOff = LCOS(*params);

            break;

        case GL_CONSTANT_ATTENUATION:
            if(*params >= 0)
                GL_LIGHTS[light & 0xF].Kc = *params;
            else
                GL_LIGHTS[light & 0xF].Kc = 0;

            break;

        case GL_LINEAR_ATTENUATION:
            if(*params >= 0)
                GL_LIGHTS[light & 0xF].Kl = *params;
            else
                GL_LIGHTS[light & 0xF].Kl = 0;

            break;

        case GL_QUADRATIC_ATTENUATION:
            if(*params >= 0)
                GL_LIGHTS[light & 0xF].Kq = *params;
            else
                GL_LIGHTS[light & 0xF].Kq = 0;

            break;

        case GL_AMBIENT_AND_DIFFUSE:
            glCopyRGBA((rgba *)params, (rgba *)&GL_LIGHTS[light & 0xF].Ka);
            glCopyRGBA((rgba *)params, (rgba *)&GL_LIGHTS[light & 0xF].Kd);
            break;
    }
}

void glLightf(GLenum light, GLenum pname, GLfloat param) {
    if(light < GL_LIGHT0 || light > GL_LIGHT0 + GL_KOS_MAX_LIGHTS) return;

    switch(pname) {
        case GL_CONSTANT_ATTENUATION:
            if(param >= 0)
                GL_LIGHTS[light & 0xF].Kc = param;
            else
                GL_LIGHTS[light & 0xF].Kc = 0;

            break;

        case GL_LINEAR_ATTENUATION:
            if(param >= 0)
                GL_LIGHTS[light & 0xF].Kl = param;
            else
                GL_LIGHTS[light & 0xF].Kl = 0;

            break;

        case GL_QUADRATIC_ATTENUATION:
            if(param >= 0)
                GL_LIGHTS[light & 0xF].Kq = param;
            else
                GL_LIGHTS[light & 0xF].Kq = 0;

            break;

        case GL_SPOT_CUTOFF:
            if(param >= 0.0f && param <= 180.0f)
                GL_LIGHTS[light & 0xF].CutOff = LCOS(param);

            break;

        case GL_SPOT_EXPONENT:
            GL_LIGHTS[light & 0xF].Exponent = param;
            break;
    }
}

/* GL Material Parameters **************************************************/

void glMateriali(GLenum face, GLenum pname, const GLint param) {
    //if(face!=GL_FRONT_AND_BACK) return;

    if(pname == GL_SHININESS) {
        if(param < 0)
            GL_MATERIAL.Shine = 0;
        else if(param > 128)
            GL_MATERIAL.Shine = 128;
        else
            GL_MATERIAL.Shine = param;
    }
}

void glMaterialf(GLenum face, GLenum pname, const GLfloat param) {
    //if(face!=GL_FRONT_AND_BACK) return;

    if(pname == GL_SHININESS) {
        if(param < 0)
            GL_MATERIAL.Shine = 0;
        else if(param > 128.0)
            GL_MATERIAL.Shine = 128.0;
        else
            GL_MATERIAL.Shine = param;
    }
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    //if(face!=GL_FRONT_AND_BACK) return;

    switch(pname) {
        case GL_AMBIENT:
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Ka[0]);
            break;

        case GL_DIFFUSE:
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Kd[0]);
            break;

        case GL_SPECULAR:
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Ks[0]);
            break;

        case GL_EMISSION:
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Ke[0]);
            break;

        case GL_SHININESS:
            glMaterialf(GL_FRONT_AND_BACK, pname, *params);
            break;

        case GL_AMBIENT_AND_DIFFUSE:
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Ka[0]);
            glCopyRGBA((rgba *)params, (rgba *)&GL_MATERIAL.Kd[0]);
            break;
    }

}

/* Vertex Lighting **********************************************************/

/* Fast POW Implementation - Less accurate, but much faster than math.h */
#define EXP_A 184
#define EXP_C 16249

float FEXP(float y) {
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

float FLOG(float y) {
    int *nTemp = (int *)&y;
    y = (*nTemp) >> 16;
    return (y - EXP_C) / EXP_A;
}

float FPOW(float b, float p) {
    return FEXP(FLOG(b) * p);
}

/* End FPOW Implementation */

void _glKosVertex3flv(const GLfloat *xyz) {
    glVertex *v = _glKosArrayBufPtr();

    v->pos[0] = xyz[0];
    v->pos[1] = xyz[1];
    v->pos[2] = xyz[2];
    v->norm[0] = GL_VERTEX_NORMAL[0];
    v->norm[1] = GL_VERTEX_NORMAL[1];
    v->norm[2] = GL_VERTEX_NORMAL[2];

    _glKosArrayBufIncrement();

    _glKosVertex3fsv(xyz);
}

void _glKosVertex3fl(GLfloat x, GLfloat y, GLfloat z) {
    glVertex *v = _glKosArrayBufPtr();

    v->pos[0] = x;
    v->pos[1] = y;
    v->pos[2] = z;
    v->norm[0] = GL_VERTEX_NORMAL[0];
    v->norm[1] = GL_VERTEX_NORMAL[1];
    v->norm[2] = GL_VERTEX_NORMAL[2];

    _glKosArrayBufIncrement();

    _glKosVertex3fs(x, y, z);
}

void _glKosVertex3flcv(const GLfloat *xyz) {
    glVertex *v = _glKosArrayBufPtr();

    v->pos[0] = xyz[0];
    v->pos[1] = xyz[1];
    v->pos[2] = xyz[2];
    v->norm[0] = GL_VERTEX_NORMAL[0];
    v->norm[1] = GL_VERTEX_NORMAL[1];
    v->norm[2] = GL_VERTEX_NORMAL[2];

    _glKosArrayBufIncrement();

    _glKosVertex3fcv(xyz);
}

void _glKosVertex3flc(GLfloat x, GLfloat y, GLfloat z) {
    glVertex *v = _glKosArrayBufPtr();

    v->pos[0] = x;
    v->pos[1] = y;
    v->pos[2] = z;
    v->norm[0] = GL_VERTEX_NORMAL[0];
    v->norm[1] = GL_VERTEX_NORMAL[1];
    v->norm[2] = GL_VERTEX_NORMAL[2];

    _glKosArrayBufIncrement();

    _glKosVertex3fc(x, y, z);
}

/**** Compute Vertex Light Color  ***/
void _glKosVertexLights(glVertex *P, pvr_vertex_t *v, GLuint count) {
#ifdef GL_ENABLE_SPECULAR
    float S;
#endif
    unsigned char i;
    float L[4] __attribute__((aligned(8)));
    float C[3] = { 0, 0, 0 };

    colorui *color = (colorui *)&v->argb;

    /* Compute Global Ambient */
    float A[3] = { GL_MATERIAL.Ke[0] + GL_MATERIAL.Ka[0] *GL_GLOBAL_AMBIENT[0],
                   GL_MATERIAL.Ke[1] + GL_MATERIAL.Ka[1] *GL_GLOBAL_AMBIENT[1],
                   GL_MATERIAL.Ke[2] + GL_MATERIAL.Ka[2] *GL_GLOBAL_AMBIENT[2]
                 };

    while(count--) {
        for(i = 0; i < GL_KOS_MAX_LIGHTS; i++)
            if(GL_LIGHT_ENABLED & 1 << i)
                if(_glKosSpotlight(&GL_LIGHTS[i], P, L)) {   /* Compute Spot / Diffuse */
                    C[0] = A[0] + (GL_MATERIAL.Kd[0] * GL_LIGHTS[i].Kd[0] * L[3]);
                    C[1] = A[1] + (GL_MATERIAL.Kd[1] * GL_LIGHTS[i].Kd[1] * L[3]);
                    C[2] = A[2] + (GL_MATERIAL.Kd[2] * GL_LIGHTS[i].Kd[2] * L[3]);

#ifdef GL_ENABLE_SPECULAR
                    S = _glKosSpecular(P, GL_EYE_POSITION, L);   /* Compute Specular */

                    if(S > 0) {
#ifdef GL_ENABLE_FAST_POW
                        S = FPOW(S, GL_MATERIAL.Shine);
#else
                        S = pow(S, GL_MATERIAL.Shine);
#endif
                        C[0] += (GL_MATERIAL.Ks[0] * GL_LIGHTS[i].Ks[0] * S);
                        C[1] += (GL_MATERIAL.Ks[1] * GL_LIGHTS[i].Ks[1] * S);
                        C[2] += (GL_MATERIAL.Ks[2] * GL_LIGHTS[i].Ks[2] * S);
                    }

#endif
                }

        color->a = 0xFF; /* Clamp / Pack Floating Point Colors to 32bit int */
        (C[0] > 1.0f) ? (color->r = 0xFF) : (color->r = (unsigned char)(255 * C[0]));
        (C[1] > 1.0f) ? (color->g = 0xFF) : (color->g = (unsigned char)(255 * C[1]));
        (C[2] > 1.0f) ? (color->b = 0xFF) : (color->b = (unsigned char)(255 * C[2]));
        color += 8; /* pvr_vertex_t color stride */
        ++P;
    }
}

void _glKosVertexLight(glVertex *P, pvr_vertex_t *v) {
#ifdef GL_ENABLE_SPECULAR
    float S;
#endif
    unsigned char i;
    float L[4] __attribute__((aligned(8)));

    /* Compute Ambient */
    float C[3] = { GL_MATERIAL.Ke[0] + GL_MATERIAL.Ka[0] *GL_GLOBAL_AMBIENT[0],
                   GL_MATERIAL.Ke[1] + GL_MATERIAL.Ka[1] *GL_GLOBAL_AMBIENT[1],
                   GL_MATERIAL.Ke[2] + GL_MATERIAL.Ka[2] *GL_GLOBAL_AMBIENT[2]
                 };

    for(i = 0; i < GL_KOS_MAX_LIGHTS; i++)
        if(GL_LIGHT_ENABLED & 1 << i)
            if(_glKosSpotlight(&GL_LIGHTS[i], P, L)) {   /* Compute Spot / Diffuse */
                C[0] += (GL_MATERIAL.Kd[0] * GL_LIGHTS[i].Kd[0] * L[3]);
                C[1] += (GL_MATERIAL.Kd[1] * GL_LIGHTS[i].Kd[1] * L[3]);
                C[2] += (GL_MATERIAL.Kd[2] * GL_LIGHTS[i].Kd[2] * L[3]);

#ifdef GL_ENABLE_SPECULAR
                S = _glKosSpecular(P, GL_EYE_POSITION, L);   /* Compute Specular */

                if(S > 0) {
#ifdef GL_ENABLE_FAST_POW
                    S = FPOW(S, GL_MATERIAL.Shine);
#else
                    S = pow(S, GL_MATERIAL.Shine);
#endif
                    C[0] += (GL_MATERIAL.Ks[0] * GL_LIGHTS[i].Ks[0] * S);
                    C[1] += (GL_MATERIAL.Ks[1] * GL_LIGHTS[i].Ks[1] * S);
                    C[2] += (GL_MATERIAL.Ks[2] * GL_LIGHTS[i].Ks[2] * S);
                }

#endif
            }

    colorui *col = (colorui *)&v->argb; /* Clamp / Pack floats to a 32bit int */
    col->a = 0xFF;
    (C[0] > 1.0f) ? (col->r = 0xFF) : (col->r = (unsigned char)(255 * C[0]));
    (C[1] > 1.0f) ? (col->g = 0xFF) : (col->g = (unsigned char)(255 * C[1]));
    (C[2] > 1.0f) ? (col->b = 0xFF) : (col->b = (unsigned char)(255 * C[2]));
}

GLuint _glKosVertexLightColor(glVertex *P) {
#ifdef GL_ENABLE_SPECULAR
    float S;
#endif
    GLuint color;
    GLubyte i;
    float L[4] __attribute__((aligned(8)));

    /* Compute Ambient */
    float C[3] = { GL_MATERIAL.Ke[0] + GL_MATERIAL.Ka[0] *GL_GLOBAL_AMBIENT[0],
                   GL_MATERIAL.Ke[1] + GL_MATERIAL.Ka[1] *GL_GLOBAL_AMBIENT[1],
                   GL_MATERIAL.Ke[2] + GL_MATERIAL.Ka[2] *GL_GLOBAL_AMBIENT[2]
                 };

    for(i = 0; i < GL_KOS_MAX_LIGHTS; i++)
        if(GL_LIGHT_ENABLED & 1 << i)
            if(_glKosSpotlight(&GL_LIGHTS[i], P, L)) {   /* Compute Spot / Diffuse */
                C[0] += (GL_MATERIAL.Kd[0] * GL_LIGHTS[i].Kd[0] * L[3]);
                C[1] += (GL_MATERIAL.Kd[1] * GL_LIGHTS[i].Kd[1] * L[3]);
                C[2] += (GL_MATERIAL.Kd[2] * GL_LIGHTS[i].Kd[2] * L[3]);

#ifdef GL_ENABLE_SPECULAR
                S = _glKosSpecular(P, GL_EYE_POSITION, L);   /* Compute Specular */

                if(S > 0) {
#ifdef GL_ENABLE_FAST_POW
                    S = FPOW(S, GL_MATERIAL.Shine);
#else
                    S = pow(S, GL_MATERIAL.Shine);
#endif
                    C[0] += (GL_MATERIAL.Ks[0] * GL_LIGHTS[i].Ks[0] * S);
                    C[1] += (GL_MATERIAL.Ks[1] * GL_LIGHTS[i].Ks[1] * S);
                    C[2] += (GL_MATERIAL.Ks[2] * GL_LIGHTS[i].Ks[2] * S);
                }

#endif
            }

    colorui *col = (colorui *)&color; /* Clamp / Pack floats to a 32bit int */
    col->a = 0xFF;
    (C[0] > 1.0f) ? (col->r = 0xFF) : (col->r = (unsigned char)(255 * C[0]));
    (C[1] > 1.0f) ? (col->g = 0xFF) : (col->g = (unsigned char)(255 * C[1]));
    (C[2] > 1.0f) ? (col->b = 0xFF) : (col->b = (unsigned char)(255 * C[2]));

    return color;
}

/** Iterate vertices submitted and compute vertex lighting **/

void _glKosVertexComputeLighting(pvr_vertex_t *v, int verts) {
    unsigned int i;
    glVertex *s = _glKosArrayBufAddr();

    _glKosMatrixLoadModelView();

    for(i = 0; i < verts; i++) {
        mat_trans_single3_nodiv(s->pos[0], s->pos[1], s->pos[2]);
        ++s;
    }

    s = _glKosArrayBufAddr();

    _glKosMatrixLoadModelRot();

    for(i = 0; i < verts; i++) {
        mat_trans_normal3(s->norm[0], s->norm[1], s->norm[2]);
        _glKosVertexLight(s++, v++);
    }
}

void _glKosLightTransformScreenSpace(float *xyz) {
    _glKosMatrixApplyScreenSpace();
    mat_trans_single(xyz[0], xyz[1], xyz[2]);
    _glKosMatrixLoadRender();
}
