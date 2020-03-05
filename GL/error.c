/* KallistiGL for KallistiOS ##version##

   libgl/gl-error.c
   Copyright (C) 2014 Josh Pearson
   Copyright (C) 2016 Lawrence Sebald
 * Copyright (C) 2017 Luke Benstead

    KOS Open GL State Machine Error Code Implementation.
*/

#include "gl.h"

#include <stdio.h>

static GLenum last_error = GL_NO_ERROR;
static char error_function[64] = { '\0' };

/* Quoth the GL Spec:
    When an error occurs, the error flag is set to the appropriate error code
    value. No other errors are recorded until glGetError is called, the error
    code is returned, and the flag is reset to GL_NO_ERROR.

   So, we only record an error here if the error code is currently unset.
   Nothing in the spec requires recording multiple error flags, although it is
   allowed by the spec. We take the easy way out for now. */

void _glKosThrowError(GLenum error, const char *function) {
    if(last_error == GL_NO_ERROR) {
        last_error = error;
        sprintf(error_function, "%s\n", function);
    }
}

GLubyte _glKosHasError() {
    return (last_error != GL_NO_ERROR) ? GL_TRUE : GL_FALSE;
}

static void _glKosResetError() {
    last_error = GL_NO_ERROR;
    sprintf(error_function, "\n");
}

static const char* _glErrorEnumAsString(GLenum error) {
    switch(error) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        default:
            return "GL_UNKNOWN_ERROR";
    }
}

void _glKosPrintError() {
    if(!_glKosHasError()) {
        return;
    }

    fprintf(stderr, "GL ERROR: %s when calling %s\n", _glErrorEnumAsString(last_error), error_function);
    _glKosResetError();
}

GLenum glGetError(void) {
    GLenum rv = last_error;
    _glKosResetError();
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
