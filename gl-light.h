/* KallistiGL for KallistiOS ##version##

   libgl/gl-light.h
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Dynamic Vertex Lighting.

   By default, specular lighting is enabled.
   For now, specular can be disabled by setting GL_ENABLE_SPECULAR
   below when you build the library.
   By default, specular lighting uses a fast POW implementation, at
   sacrifice of accuracy.  Change GL_ENABLE_FAST_POW to suit your needs.
*/

#ifndef GL_LIGHT_H
#define GL_LIGHT_H

#include "gl-sh4.h"

#define GL_ENABLE_SPECULAR 1
#define GL_ENABLE_FAST_POW 1

int _glKosSpotlight(void *glLight, void *vertex6f, void *Lvectorout);
float _glKosSpecular(void *vertex6f, void *eyepos, void *Lvectorin);

typedef struct {
    float r, g, b, a;
} rgba;

typedef struct {
    float Ke[4]; /* RGBA material emissive color      # 0.0, 0.0, 0.0, 1.0 */
    float Ka[4]; /* RGBA material ambient reflectance # 0.2, 0.2, 0.2, 1.0 */
    float Kd[4]; /* RGBA material diffuse reflectance # 0.8, 0.8, 0.8, 1.0 */
    float Ks[4]; /* RGBA material diffuse reflectance # 0.0, 0.0, 0.0, 1.0 */
    float Shine; /* Material Specular Shine           # 0.0f */
} glMaterial;

typedef struct {
    float Pos[4];   /* XYZW Position of Light           # 0.0, 0.0, 1.0, 0.0 */
    float Dir[3];   /* Spot Light Direction             # 0.0, 0.0, -1.0 */
    float CutOff;   /* Spot Light CutOff                #-1.0f */
    float Kc,       /* Constant Attenuation             # 1.0f */
          Kl,       /* Linear Attenuation               # 0.0f */
          Kq;       /* Quadratic Attenuation            # 0.0f */
    float Exponent; /* Spot Light Exponent              # 0.0f */
    float Kd[4];    /* RGBA Diffuse Light Contribution  # 1.0, 1.0, 1.0, 1.0 */
    float Ks[4];    /* RGBA Specular Light Contribution # 1.0, 1.0, 1.0, 1.0 */
    float Ka[4];    /* RGBA Ambient Light Contribution  # 0.0, 0.0, 0.0, 1.0 */
} glLight;

#endif
