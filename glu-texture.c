/* KallistiGL for KallistiOS ##version##

   libgl/glu-texture.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   A set of functions for working with ARGB pixel data, used by gluBuild2DMipmaps(...).
*/

#include "gl.h"
#include "gl-api.h"
#include "gl-rgb.h"
#include "glu.h"

GLAPI GLuint APIENTRY  glKosMipMapTexSize(GLuint width, GLuint height) {
    GLuint b = 0;

    while(width >= 1 && height >= 1) {
        b += width * height * 2;

        if(width >= 1)
            width /= 2;

        if(height >= 1)
            height /= 2;
    }

    return b;
}

static GLint gluBuild2DBiMipmaps(GLenum target, GLint internalFormat, GLsizei width,
                          GLsizei height, GLenum format, GLenum type, const void *data) {
    if(target != GL_TEXTURE_2D)
        return -1;

    if(width < 1 || height < 1)
        return 0;

    uint32 i = 0;
    uint16 x , y;

    uint16 *src = (uint16 *)data;
    uint16 *dst = (uint16 *)data + (width * height);

    for(y = 0; y < height; y += 2) {
        for(x = 0; x < width; x += 2) {
            switch(type) {
                case GL_UNSIGNED_SHORT_5_6_5:
                    dst[i++] = __glKosAverageBiPixelRGB565(*src, *(src + 1));
                    break;

                case GL_UNSIGNED_SHORT_4_4_4_4:
                    dst[i++] = __glKosAverageBiPixelARGB4444(*src, *(src + 1));
                    break;

                case GL_UNSIGNED_SHORT_1_5_5_5:
                    dst[i++] = __glKosAverageBiPixelARGB1555(*src, *(src + 1));
                    break;
            }

            src += 2;
        }

        src += width;
    }

    return gluBuild2DBiMipmaps(target, internalFormat, width / 2, height / 2,
                               format, type, (uint16 *)data + (width * height));
}

GLint APIENTRY gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width,
                        GLsizei height, GLenum format, GLenum type, const void *data) {
    if(target != GL_TEXTURE_2D)
        return -1;
	
	if(type != GL_UNSIGNED_SHORT_5_6_5 && type != GL_UNSIGNED_SHORT_4_4_4_4
		&& type != GL_UNSIGNED_SHORT_1_5_5_5)
		return -1;

	if(width < 1 || height < 1)
        return 0;

    if(width == 1 || height == 1)
        return gluBuild2DBiMipmaps(target, internalFormat, width, height, format, type, data);

    uint32 i = 0;
    uint16 x, y;

    uint16 *src = (uint16 *)data;
    uint16 *dst = (uint16 *)data + (width * height);

    for(y = 0; y < height; y += 2) {
        for(x = 0; x < width; x += 2) {
            switch(type) {
                case GL_UNSIGNED_SHORT_5_6_5:
                    dst[i++] = __glKosAverageQuadPixelRGB565(*src, *(src + 1), *(src + width), *(src + width + 1));
                    break;

                case GL_UNSIGNED_SHORT_4_4_4_4:
                    dst[i++] = __glKosAverageQuadPixelARGB4444(*src, *(src + 1), *(src + width), *(src + width + 1));
                    break;

                case GL_UNSIGNED_SHORT_1_5_5_5:
                    dst[i++] = __glKosAverageQuadPixelARGB1555(*src, *(src + 1), *(src + width), *(src + width + 1));
                    break;
            }

            src += 2;
        }

        src += width;
    }

    return gluBuild2DMipmaps(target, internalFormat, width / 2, height / 2,
                             format, type, (uint16 *)data + (width * height));
}