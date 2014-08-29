/* KallistiGL for KallistiOS ##version##

   libgl/gl-fog.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Functionality adapted from the original KOS libgl fog code:
   Copyright (C) 2002 Benoit Miller

   OpenGL Fog - Wrapper for the PowerVR fog functions in KOS.
*/

#include "gl.h"

static GLuint  FOG_MODE    = GL_EXP; /* GL_LINEAR, GL_EXP, or GL_EXP2 FOG */
static GLfloat FOG_DENSITY = 1.0f,   /* Density - GL_EXP, or GL_EXP2 FOG  */
               FOG_START   = 0.0f,   /* Linear FOG                        */
               FOG_END     = 1.0f;   /* Linear FOG                        */

void glFogi(GLenum pname, GLint param) {
    switch(pname) {
        case GL_FOG_MODE:
            switch(param) {
                case GL_LINEAR:
                    return pvr_fog_table_linear(FOG_START, FOG_END);

                case GL_EXP:
                    return pvr_fog_table_exp(FOG_DENSITY);

                case GL_EXP2:
                    return pvr_fog_table_exp2(FOG_DENSITY);

                default:
                    return;
            }

        default:
            return;
    }
}

void glFogf(GLenum pname, GLfloat param) {
    switch(pname) {
        case GL_FOG_START:
            FOG_START = param;

            if(FOG_MODE == GL_LINEAR)
                return pvr_fog_table_linear(FOG_START, FOG_END);

            return;

        case GL_FOG_END:
            FOG_END = param;

            if(FOG_MODE == GL_LINEAR)
                return pvr_fog_table_linear(FOG_START, FOG_END);

            return;

        case GL_FOG_DENSITY:
            FOG_DENSITY = param;

            if(FOG_MODE == GL_EXP)
                return pvr_fog_table_exp(FOG_DENSITY);

            if(FOG_MODE == GL_EXP2)
                return pvr_fog_table_exp2(FOG_DENSITY);

            return;

        default:
            return;
    }
}

void glFogfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
        case GL_FOG_MODE:
            return glFogi(pname, (GLint) * params);

        case GL_FOG_DENSITY:
            FOG_DENSITY = *params;

            if(FOG_MODE == GL_EXP)
                return pvr_fog_table_exp(FOG_DENSITY);

            if(FOG_MODE == GL_EXP2)
                return pvr_fog_table_exp2(FOG_DENSITY);

            return;

        case GL_FOG_START:
        case GL_FOG_END:
            return glFogf(pname, *params);

        case GL_FOG_COLOR:
            return pvr_fog_table_color(params[3], params[0], params[1], params[2]);

        case GL_FOG_INDEX:
        default:
            return;
    }
}
