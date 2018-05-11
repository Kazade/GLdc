/* KallistiGL for KallistiOS ##version##

   libgl/gl-texture.c
   Copyright (C) 2014 Josh Pearson
   Copyright (C) 2016 Joe Fenton

   Open GL Texture Submission implementation.
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "glext.h"
#include "gl-api.h"
#include "gl-rgb.h"
#include "containers/named_array.h"

//========================================================================================//
//== Internal KOS Open GL Texture Unit Structures / Global Variables ==//

#define GL_KOS_MAX_TEXTURE_UNITS 2


static GL_TEXTURE_OBJECT *GL_KOS_TEXTURE_UNIT[GL_KOS_MAX_TEXTURE_UNITS] = { NULL, NULL };
static NamedArray TEXTURE_OBJECTS;

static GLubyte GL_KOS_ACTIVE_TEXTURE = GL_TEXTURE0_ARB & 0xF;

//========================================================================================//


GLuint _glKosTextureWidth(GLuint index) {
    GL_TEXTURE_OBJECT *tex = (GL_TEXTURE_OBJECT*) named_array_get(&TEXTURE_OBJECTS, index);
    return tex->width;
}

GLuint _glKosTextureHeight(GLuint index) {
    GL_TEXTURE_OBJECT *tex = (GL_TEXTURE_OBJECT*) named_array_get(&TEXTURE_OBJECTS, index);
    return tex->height;
}

GLvoid *_glKosTextureData(GLuint index) {
    GL_TEXTURE_OBJECT *tex = (GL_TEXTURE_OBJECT*) named_array_get(&TEXTURE_OBJECTS, index);
    return tex->data;
}

void _glKosCompileHdrTx() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF] ?
           _glKosCompileHdrT(GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF]) : _glKosCompileHdr();
}

GL_TEXTURE_OBJECT *_glKosBoundTexObject() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF];
}

GL_TEXTURE_OBJECT *_glKosBoundMultiTexObject() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE1_ARB & 0xF];
}

GLuint _glKosBoundMultiTexID() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE1_ARB & 0xF] ?
           GL_KOS_TEXTURE_UNIT[GL_TEXTURE1_ARB & 0xF]->index : 0;
}

GLuint _glKosBoundTexID() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF] ?
           GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF]->index : 0;
}

GLuint _glKosActiveTextureBoundTexID() {
    return (GL_KOS_ACTIVE_TEXTURE) ? _glKosBoundMultiTexID() : _glKosBoundTexID();
}

GLubyte _glKosMaxTextureUnits() {
    return GL_KOS_MAX_TEXTURE_UNITS;
}

//========================================================================================//
//== Public KOS Open GL API Texture Unit Functionality ==//




