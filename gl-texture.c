/* KallistiGL for KallistiOS ##version##

   libgl/gl-texture.c
   Copyright (C) 2014 Josh Pearson
   Copyright (C) 2016 Joe Fenton

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

    if(internalformat != GL_UNSIGNED_SHORT_5_6_5_VQ_EXT)
        if(internalformat != GL_UNSIGNED_SHORT_5_6_5_VQ_TWID_EXT)
            if(internalformat != GL_UNSIGNED_SHORT_4_4_4_4_VQ_EXT)
                if(internalformat != GL_UNSIGNED_SHORT_4_4_4_4_VQ_TWID_EXT)
                    if(internalformat != GL_UNSIGNED_SHORT_1_5_5_5_VQ_EXT)
                        if(internalformat != GL_UNSIGNED_SHORT_1_5_5_5_VQ_TWID_EXT)
                            _glKosThrowError(GL_INVALID_OPERATION, "glCompressedTexImage2D");

    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] == NULL)
        _glKosThrowError(GL_INVALID_OPERATION, "glCompressedTexImage2D");

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->width   = width;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->height  = height;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->mip_map = level;
    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color   = internalformat;

    /* Odds are slim new data is same size as old, so free always */
    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data)
        pvr_mem_free(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data);

    GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data = pvr_mem_malloc(imageSize);

    if(data)
        sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, data, imageSize);
}

static GLint _cleanInternalFormat(GLint internalFormat) {
    switch (internalFormat) {
    case GL_ALPHA:
/*    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:*/
       return GL_ALPHA;
    case 1:
    case GL_LUMINANCE:
/*    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:*/
       return GL_LUMINANCE;
    case 2:
    case GL_LUMINANCE_ALPHA:
/*    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16: */
       return GL_LUMINANCE_ALPHA;
/*    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
       return GL_INTENSITY; */
    case 3:
       return GL_RGB;
    case GL_RGB:
/*    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16: */
       return GL_RGB;
    case 4:
       return GL_RGBA;
    case GL_RGBA:
/*    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16: */
        return GL_RGBA;

    /* Support ARB_texture_rg */
    case GL_RED:
/*    case GL_R8:
    case GL_R16:
    case GL_RED:
    case GL_COMPRESSED_RED: */
        return GL_RED;
/*    case GL_RG:
    case GL_RG8:
    case GL_RG16:
    case GL_COMPRESSED_RG:
        return GL_RG;*/
    default:
        return -1;
    }
}

static GLuint _determinePVRFormat(GLint internalFormat, GLenum type) {
    /* Given a cleaned internalFormat, return the Dreamcast format
     * that can hold it
     */
    switch(internalFormat) {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RGBA:
        /* OK so if we have something that requires alpha, we return 4444 unless
         * the type was already 1555 (1-bit alpha) in which case we return that
         */
            return (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) ?
                PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED :
                PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED;
        case GL_RED:
        case GL_RGB:
            /* No alpha? Return RGB565 which is the best we can do without using palettes */
            return PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED;
        default:
            return 0;
    }
}


typedef void (*TextureConversionFunc)(GLbyte*, GLshort*);

static void _rgba8888_to_argb4444(GLbyte* source, GLshort* dest) {
    *dest = (
        (source[3] & 0x11110000) << 8 |
        (source[0] & 0x11110000) << 4 |
        (source[1] & 0x11110000) |
        (source[2] & 0x11110000) >> 4
    );
}

static void _rgb888_to_rgb565(GLbyte* source, GLshort* dest) {
    *dest = ((source[0] & 0b11111000) << 8) | ((source[1] & 0b11111100) << 3) | (source[2] >> 3);
}

static void _r8_to_rgb565(GLbyte* source, GLshort* dest) {
    *dest = (source[0] & 0b11111000) << 8;
}

static TextureConversionFunc _determineConversion(GLint pvr_format, GLenum format, GLenum type) {
    switch(pvr_format) {
    case PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED: {
        if(type == GL_UNSIGNED_BYTE && format == GL_RGB) {
            return _rgb888_to_rgb565;
        } else if(type == GL_UNSIGNED_BYTE && format == GL_RED) {
            return _r8_to_rgb565;
        }
    } break;
    case PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED: {
        if(type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
            return _rgba8888_to_argb4444;
        }
    } break;
    default:
        break;
    }

    return 0;
}

static GLint _determineStride(GLenum format, GLenum type) {
    switch(type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return (format == GL_RED) ? 1 : (format == GL_RGB) ? 3 : 4;
    case GL_UNSIGNED_SHORT:
        return (format == GL_RED) ? 2 : (format == GL_RGB) ? 6 : 8;
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        return 2;
    }

    return -1;
}


void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *data) {
    if(target != GL_TEXTURE_2D)
        _glKosThrowError(GL_INVALID_ENUM, "glTexImage2D");

    if(format != GL_RGB)
        if(format != GL_RGBA)
            _glKosThrowError(GL_INVALID_ENUM, "glTexImage2D");

    internalFormat = _cleanInternalFormat(internalFormat);
    if(internalFormat == -1) {
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");
    }

    GLint w = width;
    if(w == 0 || (w & -w) != w) {
        /* Width is not a power of two. Must be!*/
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");
    }

    GLint h = height;
    if(h == 0 || (h & -h) != h) {
        /* height is not a power of two. Must be!*/
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");
    }

    if(level < 0)
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");

    if(border)
        _glKosThrowError(GL_INVALID_VALUE, "glTexImage2D");

    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE] == NULL)
        _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    /* Calculate the format that we need to convert the data to */
    GLuint pvr_format = _determinePVRFormat(internalFormat, type);

    if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data) {
        /* pre-existing texture - check if changed */
        if(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->width != width ||
           GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->height != height ||
           GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->mip_map != level ||
           GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color != pvr_format) {
            /* changed - free old texture memory */
            pvr_mem_free(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data);
            GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data = NULL;
        }
    }

    GLuint bytes = level ? glKosMipMapTexSize(width, height) : (width * height * 2);

    if(!GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data) {
        /* need texture memory */
        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->width   = width;
        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->height  = height;
        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->mip_map = level;
        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->color   = pvr_format;

        GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data = pvr_mem_malloc(bytes);
    }

    /* Let's assume we need to convert */
    GLboolean needsConversion = GL_TRUE;

    /*
     * These are the only formats where the source format passed in matches the pvr format.
     * Note the REV formats + GL_BGRA will reverse to ARGB which is what the PVR supports
     */
    if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_4_4_4_4_REV && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
    } else if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_1_5_5_5_REV && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
    } else if(format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 && internalFormat == GL_RGB) {
        needsConversion = GL_FALSE;
    }

    if(!data) {
        /* No data? Do nothing! */
        return;
    } else if(!needsConversion) {
        /* No conversion? Just copy the data, and the pvr_format is correct */
        sq_cpy(GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data, data, bytes);
        return;
    } else {
        TextureConversionFunc convert = _determineConversion(
            internalFormat,
            format,
            type
        );

        if(!convert) {
            _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");
            return;
        }

        GLshort* dest = GL_KOS_TEXTURE_UNIT[GL_KOS_ACTIVE_TEXTURE]->data;
        GLbyte* source = data;
        GLint stride = _determineStride(format, type);

        if(stride == -1) {
            _glKosThrowError(GL_INVALID_OPERATION, "glTexImage2D");
            return;
        }

        for(GLuint i = 0; i < bytes; ++i) {
            convert(source, dest);

            dest++;
            source += stride;
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

    if(_glKosHasError()) {
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

    if(_glKosHasError()) {

        _glKosPrintError();
        return;
    }

    GL_KOS_ACTIVE_TEXTURE = texture & 0xF;
}
