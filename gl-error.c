/* KallistiGL for KallistiOS ##version##

   libgl/gl-error.c
   Copyright (C) 2014 Josh Pearson

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

static char KOS_GL_ERROR_FUNCTION[64] = { '\0' };

void _glKosThrowError(GLenum error, char *function) {

    sprintf(KOS_GL_ERROR_FUNCTION, "%s\n", function);

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