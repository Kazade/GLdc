#include <stdio.h>
#include "private.h"
#include "../include/glkos.h"
#include "../include/glext.h"

typedef struct {
    GLuint index;
    GLuint texture_id;
    GLboolean is_complete;

    /* FIXME: Add OP, TR and PT lists per framebuffer */

} FrameBuffer;

static FrameBuffer* ACTIVE_FRAMEBUFFER = NULL;
static NamedArray FRAMEBUFFERS;


void initFramebuffers() {
    named_array_init(&FRAMEBUFFERS, sizeof(FrameBuffer), 32);
}

void wipeTextureOnFramebuffers(GLuint texture) {
    /* Spec says we don't update inactive framebuffers, they'll presumably just cause
     * a GL_INVALID_OPERATION if we try to render to them */
    if(ACTIVE_FRAMEBUFFER && ACTIVE_FRAMEBUFFER->texture_id == texture) {
        ACTIVE_FRAMEBUFFER->texture_id = 0;
    }
}

void APIENTRY glGenFramebuffersEXT(GLsizei n, GLuint* framebuffers) {
    TRACE();

    while(n--) {
        GLuint id = 0;
        FrameBuffer* fb = (FrameBuffer*) named_array_alloc(&FRAMEBUFFERS, &id);
        fb->index = id;
        fb->is_complete = GL_FALSE;
        fb->texture_id = 0;

        *framebuffers = id;
        framebuffers++;
    }
}

void APIENTRY glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) {
    TRACE();

    while(n--) {
        FrameBuffer* fb = (FrameBuffer*) named_array_get(&FRAMEBUFFERS, *framebuffers);

        if(fb == ACTIVE_FRAMEBUFFER) {
            ACTIVE_FRAMEBUFFER = NULL;
        }

        named_array_release(&FRAMEBUFFERS, *framebuffers++);
    }
}

void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer) {
    TRACE();

    if(framebuffer) {
        ACTIVE_FRAMEBUFFER = (FrameBuffer*) named_array_get(&FRAMEBUFFERS, framebuffer);
    } else {
        ACTIVE_FRAMEBUFFER = NULL;

        /* FIXME: This is where we need to submit the lists and then clear them. Binding zero means returning to the
         * default framebuffer so we need to render a frame to the texture at that point */
    }
}

void APIENTRY glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if(texture != 0 && !glIsTexture(texture)) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    if(!ACTIVE_FRAMEBUFFER) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    ACTIVE_FRAMEBUFFER->texture_id = texture;
}

static inline GLuint A1555(GLuint v) {
    const GLuint MASK = (1 << 15);
    return (v & MASK) >> 15;
}

static inline GLuint R1555(GLuint v) {
    const GLuint MASK = (31 << 10);
    return (v & MASK) >> 10;
}

static inline GLuint G1555(GLuint v) {
    const GLuint MASK = (31 << 5);
    return (v & MASK) >> 5;
}

static inline GLuint B1555(GLuint v) {
    const GLuint MASK = (31 << 0);
    return (v & MASK) >> 0;
}

static inline GLuint A4444(GLuint v) {
    const GLuint MASK = (0xF << 12);
    return (v & MASK) >> 12;
}

static inline GLuint R4444(GLuint v) {
    const GLuint MASK = (0xF << 8);
    return (v & MASK) >> 8;
}

static inline GLuint G4444(GLuint v) {
    const GLuint MASK = (0xF << 4);
    return (v & MASK) >> 4;
}

static inline GLuint B4444(GLuint v) {
    const GLuint MASK = (0xF << 0);
    return (v & MASK) >> 0;
}

static inline GLuint R565(GLuint v) {
    const GLuint MASK = (31 << 11);
    return (v & MASK) >> 11;
}

static inline GLuint G565(GLuint v) {
    const GLuint MASK = (63 << 5);
    return (v & MASK) >> 5;
}

static inline GLuint B565(GLuint v) {
    const GLuint MASK = (31 << 0);
    return (v & MASK) >> 0;
}

GLboolean _glCalculateAverageTexel(const GLubyte* src, const GLuint srcWidth, const GLuint pvrFormat, GLubyte* dest) {
    GLushort* s1 = ((GLushort*) src);
    GLushort* s2 = ((GLushort*) src) + 1;
    GLushort* s3 = ((GLushort*) src) + srcWidth;
    GLushort* s4 = ((GLushort*) src) + srcWidth + 1;
    GLushort* d1 = ((GLushort*) dest);

    GLuint a, r, g, b;

    if((pvrFormat & PVR_TXRFMT_ARGB1555) == PVR_TXRFMT_ARGB1555) {
        a = A1555(*s1) + A1555(*s2) + A1555(*s3) + A1555(*s4);
        r = R1555(*s1) + R1555(*s2) + R1555(*s3) + R1555(*s4);
        g = G1555(*s1) + R1555(*s2) + R1555(*s3) + R1555(*s4);
        b = B1555(*s1) + R1555(*s2) + R1555(*s3) + R1555(*s4);

        a /= 4;
        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = (a << 15 | r << 10 | g << 5 | b);
    } else if((pvrFormat & PVR_TXRFMT_ARGB4444) == PVR_TXRFMT_ARGB4444) {
        a = A4444(*s1) + A4444(*s2) + A4444(*s3) + A4444(*s4);
        r = R4444(*s1) + R4444(*s2) + R4444(*s3) + R4444(*s4);
        g = G4444(*s1) + R4444(*s2) + R4444(*s3) + R4444(*s4);
        b = B4444(*s1) + R4444(*s2) + R4444(*s3) + R4444(*s4);

        a /= 4;
        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = (a << 12 | r << 8 | g << 4 | b);
    } else if((pvrFormat & PVR_TXRFMT_RGB565) == PVR_TXRFMT_ARGB4444) {
        r = R565(*s1) + R565(*s2) + R565(*s3) + R565(*s4);
        g = G565(*s1) + R565(*s2) + R565(*s3) + R565(*s4);
        b = B565(*s1) + R565(*s2) + R565(*s3) + R565(*s4);

        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = (r << 11 | g << 5 | b);
    } else {
        fprintf(stderr, "ERROR: Unsupported PVR format for mipmap generation");
        _glKosThrowError(GL_INVALID_OPERATION, "glGenerateMipmapEXT");
        _glKosPrintError();
        return GL_FALSE;
    }

    return GL_TRUE;
}

void APIENTRY glGenerateMipmapEXT(GLenum target) {
    if(target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    TextureObject* tex = getBoundTexture();

    if(!tex || !tex->data || !tex->mipmapCount) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    if(_glIsMipmapComplete(tex)) {
        /* Nothing to do */
        return;
    }

    GLuint i = 1;
    GLuint sx, sy, dx, dy;
    GLuint prevWidth = tex->width;
    GLuint prevHeight = tex->height;

    for(; i < _glGetMipmapLevelCount(tex); ++i) {
        GLubyte* prevData = _glGetMipmapLocation(tex, i - 1);
        GLubyte* thisData = _glGetMipmapLocation(tex, i);

        GLuint thisWidth = (prevWidth > 1) ? prevWidth / 2 : 1;
        GLuint thisHeight = (prevHeight > 1) ? prevHeight / 2 : 1;

        /* Do the minification */
        for(sx = 0, dx = 0; sx < prevWidth; sx += 2, dx += 1) {
            for(sy = 0, sy = 0; sy < prevHeight; sy += 2, dy += 1) {
                GLubyte* srcTexel = &prevData[
                    ((sy * prevWidth) + sx) * tex->dataStride
                ];

                GLubyte* destTexel = &thisData[
                    ((dy * thisWidth) + dx) * tex->dataStride
                ];

                if(!_glCalculateAverageTexel(
                    srcTexel,
                    prevWidth,
                    tex->color,
                    destTexel
                )) {
                    return;
                }
            }
        }

        prevWidth = thisWidth;
        prevHeight = thisHeight;
    }
}

GLenum APIENTRY glCheckFramebufferStatusEXT(GLenum target) {
    if(target != GL_FRAMEBUFFER_EXT) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        return 0;
    }

    if(!ACTIVE_FRAMEBUFFER) {
        return GL_FRAMEBUFFER_COMPLETE_EXT;
    }

    if(!ACTIVE_FRAMEBUFFER->texture_id) {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT;
    }

    return GL_FRAMEBUFFER_COMPLETE_EXT;
}

GLboolean APIENTRY glIsFramebufferEXT(GLuint framebuffer) {
    return (named_array_used(&FRAMEBUFFERS, framebuffer)) ? GL_TRUE : GL_FALSE;
}
