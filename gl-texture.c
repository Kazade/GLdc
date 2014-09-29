/* KallistiGL for KallistiOS ##version##

   libgl/gl-texture.c
   Copyright (C) 2014 Josh Pearson

   Open GL Texture Submission implementation.
   This implementation uses a dynamic linked list to store the texture objects.
*/

#include "gl.h"
#include "gl-api.h"

#include <malloc.h>
#include <stdio.h>

//========================================================================================//
//== Internal KOS Open GL Texture Unit Structures / Global Variables ==//

static GL_TEXTURE_OBJECT *TEXTURE_OBJ        = NULL;
static GL_TEXTURE_OBJECT *GL_TEXTURE_POINTER = NULL;

#define GL_CLAMP_U (1<<1)
#define GL_CLAMP_V (1<<0)

//========================================================================================//

GLubyte _glKosInitTextures() {
    TEXTURE_OBJ = malloc(sizeof(GL_TEXTURE_OBJECT));

    if(TEXTURE_OBJ == NULL)
        return 0;

    TEXTURE_OBJ->index = 0;
    TEXTURE_OBJ->data = NULL;
    TEXTURE_OBJ->link = NULL;

    return 1;
}

static GLsizei _glKosGetLastTextureIndex() {
    GL_TEXTURE_OBJECT *ptr = TEXTURE_OBJ;

    while(ptr->link != NULL)
        ptr = (GL_TEXTURE_OBJECT *)ptr->link;

    return ptr->index;
}

static void _glKosInsertTextureObj(GL_TEXTURE_OBJECT *obj) {
    GL_TEXTURE_OBJECT *ptr = TEXTURE_OBJ;

    while(ptr->link != NULL)
        ptr = (GL_TEXTURE_OBJECT *)ptr->link;

    ptr->link = obj;
}

static GL_TEXTURE_OBJECT *_glKosGetTextureObj(GLuint index) {
    GL_TEXTURE_OBJECT *ptr = TEXTURE_OBJ;

    while(ptr->link != NULL && ptr->index != index)
        ptr = (GL_TEXTURE_OBJECT *)ptr->link;

    return ptr;
}

static void _glKosBindTexture(GLuint index) {
    GL_TEXTURE_POINTER = _glKosGetTextureObj(index);
}

static void _glKosUnbindTexture() {
    GL_TEXTURE_POINTER = NULL;
}

GLuint _glKosTextureWidth(GLuint index) {
    GL_TEXTURE_OBJECT *tex = _glKosGetTextureObj(index);
    return tex->width;
}

GLuint _glKosTextureHeight(GLuint index) {
    GL_TEXTURE_OBJECT *tex = _glKosGetTextureObj(index);
    return tex->height;
}

GLvoid *_glKosTextureData(GLuint index) {
    GL_TEXTURE_OBJECT *tex = _glKosGetTextureObj(index);
    return tex->data;
}

void _glKosCompileHdrTx() {
    return GL_TEXTURE_POINTER ? _glKosCompileHdrT(GL_TEXTURE_POINTER) : _glKosCompileHdr();
}

GLuint _glKosBoundTexID() {
    return GL_TEXTURE_POINTER ? GL_TEXTURE_POINTER->index : 0;
}

//========================================================================================//
//== Public KOS Open GL API Texture Unit Functionality ==//

void APIENTRY glGenTextures(GLsizei n, GLuint *textures) {
    GLsizei index = _glKosGetLastTextureIndex();

    while(n--) {
        GL_TEXTURE_OBJECT *txr = malloc(sizeof(GL_TEXTURE_OBJECT));
        txr->index = ++index;
        txr->data = NULL;
        txr->link = NULL;

        txr->width = txr->height = 0;
        txr->mip_map = 0;
        txr->uv_clamp = 0;
        txr->env = PVR_TXRENV_MODULATEALPHA;
        txr->filter = PVR_FILTER_NONE;

        _glKosInsertTextureObj(txr);

        *textures++ = txr->index;
    }
}

void APIENTRY glDeleteTextures(GLsizei n, GLuint *textures) {
    while(n--) {
        GL_TEXTURE_OBJECT *txr = TEXTURE_OBJ, *ltxr = NULL;

        while(txr->index != *textures && txr->link != NULL) {
            ltxr = txr;
            txr = txr->link;
        }

        ltxr->link = txr->link;

        if(txr->index == *textures) {
            if(GL_TEXTURE_POINTER)
                if(GL_TEXTURE_POINTER->index == txr->index)
                    GL_TEXTURE_POINTER = NULL;

            if(txr->data != NULL)
                pvr_mem_free(txr->data);

            free(txr);
        }

        ++textures;
    }
}

void APIENTRY glBindTexture(GLenum  target, GLuint texture) {
    if(target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_ENUM, "glBindTexture");
        _glKosPrintError();
        return;
    }

    texture ? _glKosBindTexture(texture) : _glKosUnbindTexture();
}

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, GLvoid *data) {
    if(target != GL_TEXTURE_2D)
        _glKosThrowError(GL_INVALID_ENUM, "glTexImage2D");

    if(format != GL_RGB)
        if(format != GL_RGBA)
            _glKosThrowError(GL_INVALID_ENUM, "glTexImage2D");

    if(internalFormat != GL_RGB)
        if(internalFormat != GL_RGBA)
            _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");

    if(level < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");

    if(border)
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");

    if(format != internalFormat)
        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(format == GL_RGB)
        if(type != GL_UNSIGNED_SHORT_5_6_5)
            if(type != GL_UNSIGNED_SHORT_5_6_5_TWID)
                if(type != GL_UNSIGNED_SHORT_5_6_5_VQ)
                    if(type != GL_UNSIGNED_SHORT_5_6_5_VQ_TWID)
                        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(format == GL_RGBA)
        if(type != GL_UNSIGNED_SHORT_4_4_4_4)
            if(type != GL_UNSIGNED_SHORT_4_4_4_4_TWID)
                if(type != GL_UNSIGNED_SHORT_4_4_4_4_VQ)
                    if(type != GL_UNSIGNED_SHORT_4_4_4_4_VQ_TWID)
                        if(type != GL_UNSIGNED_SHORT_5_5_5_1)
                            if(type != GL_UNSIGNED_SHORT_5_5_5_1_TWID)
                                if(type != GL_UNSIGNED_SHORT_5_5_5_1_VQ)
                                    if(type != GL_UNSIGNED_SHORT_5_5_5_1_VQ_TWID)
                                        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(GL_TEXTURE_POINTER == NULL)
        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    GL_TEXTURE_POINTER->width   = width;
    GL_TEXTURE_POINTER->height  = height;
    GL_TEXTURE_POINTER->mip_map = level;
    GL_TEXTURE_POINTER->color   = type;

    GLuint bytes = level ? glKosMipMapTexSize(width, height) : (width * height * 2);

    if(format & PVR_TXRFMT_VQ_ENABLE)
        GL_TEXTURE_POINTER->data = pvr_mem_malloc(bytes * 0.25);
    else
        GL_TEXTURE_POINTER->data = pvr_mem_malloc(bytes);

    if(data)
        sq_cpy(GL_TEXTURE_POINTER->data, data, bytes);
}

void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if(target == GL_TEXTURE_2D) {
        switch(pname) {
            case GL_TEXTURE_MAG_FILTER:
            case GL_TEXTURE_MIN_FILTER:
                switch(param) {
                    case GL_LINEAR:
                        GL_TEXTURE_POINTER->filter = PVR_FILTER_BILINEAR;
                        break;

                    case GL_NEAREST:
                        GL_TEXTURE_POINTER->filter = PVR_FILTER_NEAREST;
                        break;

                    case GL_FILTER_NONE:
                        GL_TEXTURE_POINTER->filter = PVR_FILTER_NONE;
                        break;

                    case GL_FILTER_BILINEAR:
                        GL_TEXTURE_POINTER->filter = PVR_FILTER_BILINEAR;
                        break;

                    default:
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_S:
                switch(param) {
                    case GL_CLAMP:
                        GL_TEXTURE_POINTER->uv_clamp |= GL_CLAMP_U;
                        break;

                    case GL_REPEAT:
                        GL_TEXTURE_POINTER->uv_clamp &= ~GL_CLAMP_U;
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_T:
                switch(param) {
                    case GL_CLAMP:
                        GL_TEXTURE_POINTER->uv_clamp |= GL_CLAMP_V;
                        break;

                    case GL_REPEAT:
                        GL_TEXTURE_POINTER->uv_clamp &= ~GL_CLAMP_V;
                        break;
                }

                break;
        }
    }
}

void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) {
    if(target != GL_TEXTURE_ENV)
        _glKosThrowError(GL_INVALID_ENUM, "glTexEnvi");

    if(pname != GL_TEXTURE_ENV_MODE)
        _glKosThrowError(GL_INVALID_ENUM, "glTexEnvi");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    if(param >= PVR_TXRENV_REPLACE && param <= PVR_TXRENV_MODULATEALPHA)
        GL_TEXTURE_POINTER->env = param;
}

void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    glTexEnvi(target, pname, param);
}
