/* KallistiGL for KallistiOS ##version##

   libgl/gl-texture.c
   Copyright (C) 2014 Josh Pearson

   Open GL Texture Submission implementation.
   This implementation uses a dynamic linked list to store the texture objects.
*/

#include "gl.h"
#include "glext.h"
#include "gl-api.h"
#include "gl-rgb.h"

#include <malloc.h>
#include <stdio.h>

//========================================================================================//
//== Internal KOS Open GL Texture Unit Structures / Global Variables ==//

#define GL_KOS_MAX_TEXTURE_UNITS 2
#define GL_KOS_CLAMP_U (1<<1)
#define GL_KOS_CLAMP_V (1<<0)

static GL_TEXTURE_OBJECT *TEXTURE_OBJ = NULL;
static GL_TEXTURE_OBJECT *GL_KOS_TEXTURE_UNIT[GL_KOS_MAX_TEXTURE_UNITS] = { NULL, NULL };

static GLubyte GL_KOS_ACTIVE_TEXTURE = GL_TEXTURE0_ARB & 0xF;

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
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] = _glKosGetTextureObj(index);
}

static void _glKosUnbindTexture() {
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] = NULL;
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
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF] ?
           _glKosCompileHdrT(GL_KOS_TEXTURE_UNIT[GL_TEXTURE0_ARB & 0xF]) : _glKosCompileHdr();
}

GL_TEXTURE_OBJECT *_glKosBoundMultiTexID() {
    return GL_KOS_TEXTURE_UNIT[GL_TEXTURE1_ARB & 0xF];
}

GLuint _glKosBoundTexID() {
    return GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] ?
           GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->index : 0;
}

GLubyte _glKosMaxTextureUnits() {
    return GL_KOS_MAX_TEXTURE_UNITS;
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
            if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE])
                if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->index == txr->index)
                    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] = NULL;

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

void APIENTRY glCompressedTexImage2D(GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLint border,
                                     GLsizei imageSize,
                                     const GLvoid *data) {
    if(target != GL_TEXTURE_2D)
        _glKosThrowError(GL_INVALID_ENUM, "glCompressedTexImage2D");

    if(level < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glCompressedTexImage2D");

    if(border)
        _glKosThrowError(GL_INVALID_VALUE, "glCompressedTexImage2D");

    if(internalformat != GL_UNSIGNED_SHORT_5_6_5_VQ)
        if(internalformat != GL_UNSIGNED_SHORT_5_6_5_VQ_TWID)
            if(internalformat != GL_UNSIGNED_SHORT_4_4_4_4_VQ)
                if(internalformat != GL_UNSIGNED_SHORT_4_4_4_4_VQ_TWID)
                    if(internalformat != GL_UNSIGNED_SHORT_1_5_5_5_VQ)
                        if(internalformat != GL_UNSIGNED_SHORT_1_5_5_5_VQ_TWID)
                            _glKosThrowError(GL_INVALID_OPERATION, "glCompressedTexImage2D");

    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] == NULL)
        _glKosThrowError(GL_INVALID_OPERATION, "glCompressedTexImage2D");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->width   = width;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->height  = height;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->mip_map = level;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color   = internalformat;

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data = pvr_mem_malloc(imageSize);

    if(data)
        sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, data, imageSize);
}

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *data) {
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

    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] == NULL)
        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(_glKosGetError()) {
        _glKosPrintError();
        return;
    }

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->width   = width;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->height  = height;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->mip_map = level;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color   = type;

    GLuint bytes = level ? glKosMipMapTexSize(width, height) : (width * height * 2);

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data = pvr_mem_malloc(bytes);

    if(data)
    {
        switch(type)
        {
            case GL_BYTE:          /* Texture Formats that need conversion for PVR */
            case GL_UNSIGNED_BYTE:
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
            case GL_FLOAT:
                 {
                     uint16 *tex;

                     tex = (uint16 *)malloc(width * height * sizeof(uint16));
                     if(!tex) {
                         _glKosThrowError(GL_OUT_OF_MEMORY, "glTexImage2D");
                         _glKosPrintError();
                         return;
                     }

                     switch(internalFormat)
                     {
                         case GL_RGB:
                              _glPixelConvertRGB(type, width, height, (void *)data, tex);
                              GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color = (PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED);
                              sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, tex, bytes);
                              break;

                         case GL_RGBA:
                              _glPixelConvertRGBA(type, width, height, (void *)data, tex);
                              GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color = (PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED);
                              sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, tex, bytes);
                              break;
                     }

                     free(tex);
                 }
                 break;

            case GL_UNSIGNED_SHORT_5_6_5:   /* Texture Formats that do not need conversion  */
            case GL_UNSIGNED_SHORT_5_6_5_TWID:
            case GL_UNSIGNED_SHORT_1_5_5_5:
            case GL_UNSIGNED_SHORT_1_5_5_5_TWID:
            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_4_4_4_4_TWID:
                 sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, data, bytes);
                 break;

            default: /* Unsupported Texture Format */
                 _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");
                 break;
        }
    }
}

void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if(target == GL_TEXTURE_2D) {
        switch(pname) {
            case GL_TEXTURE_MAG_FILTER:
            case GL_TEXTURE_MIN_FILTER:
                switch(param) {
                    case GL_LINEAR:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->filter = PVR_FILTER_BILINEAR;
                        break;

                    case GL_NEAREST:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->filter = PVR_FILTER_NEAREST;
                        break;

                    case GL_FILTER_NONE:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->filter = PVR_FILTER_NONE;
                        break;

                    case GL_FILTER_BILINEAR:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->filter = PVR_FILTER_BILINEAR;
                        break;

                    default:
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_S:
                switch(param) {
                    case GL_CLAMP:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->uv_clamp |= GL_KOS_CLAMP_U;
                        break;

                    case GL_REPEAT:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->uv_clamp &= ~GL_KOS_CLAMP_U;
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_T:
                switch(param) {
                    case GL_CLAMP:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->uv_clamp |= GL_KOS_CLAMP_V;
                        break;

                    case GL_REPEAT:
                        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->uv_clamp &= ~GL_KOS_CLAMP_V;
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
        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->env = param;
}

void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    glTexEnvi(target, pname, param);
}

void APIENTRY glActiveTextureARB(GLenum texture) {
    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + GL_KOS_MAX_TEXTURE_UNITS)
        _glKosThrowError(GL_INVALID_ENUM, "glActiveTextureARB");

    if(_glKosGetError()) {

        _glKosPrintError();
        return;
    }

    GL_KOS_ACTIVE_TEXTURE = texture & 0xF;
}
