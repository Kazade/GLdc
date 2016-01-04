/* KallistiGL for KallistiOS ##version##

   libgl/gl-error.c
   Copyright (C) 2014 Josh Pearson
   Copyright (C) 2016 Lawrence Sebald

    KOS Open GL State Machine Error Code Implementation.
*/

#include "gl.h"
#include "gl-api.h"

#include <stdio.h>

#define KOS_GL_INVALID_ENUM       (1<<0)
#define KOS_GL_OUT_OF_MEMORY      (1<<1)
#define KOS_GL_INVALID_OPERATION  (1<<2)
#define KOS_GL_INVALID_VALUE      (1<<3)

static GLsizei KOS_GL_ERROR_CODE;
static GLenum gl_last_error = GL_NO_ERROR;

static char KOS_GL_ERROR_FUNCTION[64] = { '\0' };

/* Quoth the GL Spec:
    When an error occurs, the error flag is set to the appropriate error code
    value. No other errors are recorded until glGetError is called, the error
    code is returned, and the flag is reset to GL_NO_ERROR.

   So, we only record an error here if the error code is currently unset.
   Nothing in the spec requires recording multiple error flags, although it is
   allowed by the spec. We take the easy way out for now. */
static void set_err_flag(GLenum error) {
    if(gl_last_error == GL_NO_ERROR)
        gl_last_error = error;
}

void _glKosThrowError(GLenum error, char *function) {

    sprintf(KOS_GL_ERROR_FUNCTION, "%s\n", function);
    set_err_flag(error);

    switch(error) {
        case GL_INVALID_ENUM:
            KOS_GL_ERROR_CODE |= KOS_GL_INVALID_ENUM;
            break;

        case GL_OUT_OF_MEMORY:
            KOS_GL_ERROR_CODE |= KOS_GL_OUT_OF_MEMORY;
            break;

        case GL_INVALID_OPERATION:
            KOS_GL_ERROR_CODE |= KOS_GL_INVALID_OPERATION;
            break;

        case GL_INVALID_VALUE:
            KOS_GL_ERROR_CODE |= KOS_GL_INVALID_VALUE;
            break;

    }
}

GLsizei _glKosGetError() {
    return KOS_GL_ERROR_CODE;
}

void _glKosResetError() {
    KOS_GL_ERROR_CODE = 0;
    sprintf(KOS_GL_ERROR_FUNCTION, "\n");
}

void _glKosPrintError() {
    printf("\nKOS GL ERROR THROWN BY FUNCTION: %s\n", KOS_GL_ERROR_FUNCTION);

    if(KOS_GL_ERROR_CODE & KOS_GL_INVALID_ENUM)
        printf("KOS GL ERROR: GL_INVALID_ENUM\n");

    if(KOS_GL_ERROR_CODE & KOS_GL_OUT_OF_MEMORY)
        printf("KOS GL ERROR: GL_OUT_OF_MEMORY\n");

    if(KOS_GL_ERROR_CODE & KOS_GL_INVALID_OPERATION)
        printf("KOS GL ERROR: GL_INVALID_OPERATION\n");

    if(KOS_GL_ERROR_CODE & KOS_GL_INVALID_VALUE)
        printf("KOS GL ERROR: GL_INVALID_VALUE\n");

    _glKosResetError();
}

GLenum glGetError(void) {
    GLenum rv = gl_last_error;

    gl_last_error = GL_NO_ERROR;
    return rv;
}

const GLubyte *gluErrorString(GLenum error) {
    switch(error) {
        case GL_NO_ERROR:
            return (GLubyte *)"no error";

        case GL_INVALID_ENUM:
            return (GLubyte *)"invalid enumerant";

        case GL_INVALID_OPERATION:
            return (GLubyte *)"invalid operation";

        case GL_INVALID_VALUE:
            return (GLubyte *)"invalid value";

        case GL_OUT_OF_MEMORY:
            return (GLubyte *)"out of memory";

        default:
            return (GLubyte *)"unknown error";
    }
}
