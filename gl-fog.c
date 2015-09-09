/* KallistiGL for KallistiOS ##version##

   libgl/gl-fog.c
   Copyright (C) 2013-2014 Josh Pearson

   Functionality adapted from the original KOS libgl fog code:
   Copyright (C) 2002 Benoit Miller

   OpenGL Fog - Wrapper for the PowerVR fog functions in KOS.
*/

#include "gl.h"

static GLuint  GL_KOS_FOG_MODE    = GL_EXP; /* GL_LINEAR, GL_EXP, or GL_EXP2 FOG */
static GLfloat GL_KOS_FOG_DENSITY = 1.0f,   /* Density - GL_EXP, or GL_EXP2 FOG  */
               GL_KOS_FOG_START   = 0.0f,   /* Linear FOG                        */
               GL_KOS_FOG_END     = 1.0f;   /* Linear FOG                        */

void APIENTRY glFogi(GLenum pname, GLint param) {
    switch(pname) {
        case GL_FOG_MODE:
            switch(param) {
                case GL_LINEAR:
                    pvr_fog_table_linear(GL_KOS_FOG_START, GL_KOS_FOG_END);
                    break;

                case GL_EXP:
                    pvr_fog_table_exp(GL_KOS_FOG_DENSITY);
                    break;

                case GL_EXP2:
                    pvr_fog_table_exp2(GL_KOS_FOG_DENSITY);
                    break;
            }
    }
}

void APIENTRY glFogf(GLenum pname, GLfloat param) {
    switch(pname) {
        case GL_FOG_START:
            GL_KOS_FOG_START = param;

            if(GL_KOS_FOG_MODE == GL_LINEAR)
                pvr_fog_table_linear(GL_KOS_FOG_START, GL_KOS_FOG_END);

            break;

        case GL_FOG_END:
            GL_KOS_FOG_END = param;

            if(GL_KOS_FOG_MODE == GL_LINEAR)
                pvr_fog_table_linear(GL_KOS_FOG_START, GL_KOS_FOG_END);

            break;

        case GL_FOG_DENSITY:
            GL_KOS_FOG_DENSITY = param;

            if(GL_KOS_FOG_MODE == GL_EXP)
                pvr_fog_table_exp(GL_KOS_FOG_DENSITY);

            else if(GL_KOS_FOG_MODE == GL_EXP2)
                pvr_fog_table_exp2(GL_KOS_FOG_DENSITY);

            break;
    }
}

void APIENTRY glFogfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
        case GL_FOG_MODE:
            glFogi(pname, (GLint) * params);
            break;

        case GL_FOG_DENSITY:
            GL_KOS_FOG_DENSITY = *params;

            if(GL_KOS_FOG_MODE == GL_EXP)
                pvr_fog_table_exp(GL_KOS_FOG_DENSITY);

            else if(GL_KOS_FOG_MODE == GL_EXP2)
                pvr_fog_table_exp2(GL_KOS_FOG_DENSITY);

            break;

        case GL_FOG_START:
        case GL_FOG_END:
            glFogf(pname, *params);
            break;

        case GL_FOG_COLOR:
            pvr_fog_table_color(params[3], params[0], params[1], params[2]);
            break;
    }
}
