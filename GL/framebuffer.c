#include <stdio.h>
#include <assert.h>

#include "private.h"

typedef struct {
    GLuint index;
    GLuint texture_id;
    GLboolean is_complete;

    /* FIXME: Add OP, TR and PT lists per framebuffer */

} FrameBuffer;

static FrameBuffer* ACTIVE_FRAMEBUFFER = NULL;
static NamedArray FRAMEBUFFERS;


void _glInitFramebuffers() {
    named_array_init(&FRAMEBUFFERS, sizeof(FrameBuffer), 32);

    // Reserve zero so that it is never given to anyone as an ID!
    named_array_reserve(&FRAMEBUFFERS, 0);
}

void _glWipeTextureOnFramebuffers(GLuint texture) {
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
    _GL_UNUSED(target);
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
    _GL_UNUSED(target);
    _GL_UNUSED(attachment);
    _GL_UNUSED(textarget);
    _GL_UNUSED(level);
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

GL_FORCE_INLINE GLuint A1555(GLuint v) {
    const GLuint MASK = (1 << 15);
    return (v & MASK) >> 8;
}

GL_FORCE_INLINE GLuint R1555(GLuint v) {
    const GLuint MASK = (31 << 10);
    return (v & MASK) >> 7;
}

GL_FORCE_INLINE GLuint G1555(GLuint v) {
    const GLuint MASK = (31 << 5);
    return (v & MASK) >> 2;
}

GL_FORCE_INLINE GLuint B1555(GLuint v) {
    const GLuint MASK = (31 << 0);
    return (v & MASK) << 3;
}

GL_FORCE_INLINE GLuint A4444(GLuint v) {
    const GLuint MASK = (0xF << 12);
    return (v & MASK) >> 12;
}

GL_FORCE_INLINE GLuint R4444(GLuint v) {
    const GLuint MASK = (0xF << 8);
    return (v & MASK) >> 8;
}

GL_FORCE_INLINE GLuint G4444(GLuint v) {
    const GLuint MASK = (0xF << 4);
    return (v & MASK) >> 4;
}

GL_FORCE_INLINE GLuint B4444(GLuint v) {
    const GLuint MASK = (0xF << 0);
    return (v & MASK) >> 0;
}

GL_FORCE_INLINE GLuint R565(GLuint v) {
    const GLuint MASK = (31 << 11);
    return (v & MASK) >> 8;
}

GL_FORCE_INLINE GLuint G565(GLuint v) {
    const GLuint MASK = (63 << 5);
    return (v & MASK) >> 3;
}

GL_FORCE_INLINE GLuint B565(GLuint v) {
    const GLuint MASK = (31 << 0);
    return (v & MASK) << 3;
}

static GL_NO_INSTRUMENT GLboolean _glCalculateAverageTexel(GLuint pvrFormat, const GLubyte* src1, const GLubyte* src2, const GLubyte* src3, const GLubyte* src4, GLubyte* t) {
    GLuint a, r, g, b;

    GLubyte format = ((pvrFormat & (1 << 27)) | (pvrFormat & (1 << 26))) >> 26;

    const GLubyte ARGB1555 = 0;
    const GLubyte ARGB4444 = 1;
    const GLubyte RGB565 = 2;

    if((pvrFormat & GPU_TXRFMT_PAL8BPP) == GPU_TXRFMT_PAL8BPP) {
        /* Paletted... all we can do really is just pick one of the
         * 4 texels.. unless we want to change the palette (bad) or
         * pick the closest available colour (slow, and probably bad)
         */
        *t = *src1;
    } else if(format == RGB565) {
        GLushort* s1 = (GLushort*) src1;
        GLushort* s2 = (GLushort*) src2;
        GLushort* s3 = (GLushort*) src3;
        GLushort* s4 = (GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        r = R565(*s1) + R565(*s2) + R565(*s3) + R565(*s4);
        g = G565(*s1) + G565(*s2) + G565(*s3) + G565(*s4);
        b = B565(*s1) + B565(*s2) + B565(*s3) + B565(*s4);

        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = PACK_RGB565(r, g, b);
    } else if(format == ARGB4444) {
        GLushort* s1 = (GLushort*) src1;
        GLushort* s2 = (GLushort*) src2;
        GLushort* s3 = (GLushort*) src3;
        GLushort* s4 = (GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        a = A4444(*s1) + A4444(*s2) + A4444(*s3) + A4444(*s4);
        r = R4444(*s1) + R4444(*s2) + R4444(*s3) + R4444(*s4);
        g = G4444(*s1) + G4444(*s2) + G4444(*s3) + G4444(*s4);
        b = B4444(*s1) + B4444(*s2) + B4444(*s3) + B4444(*s4);

        a /= 4;
        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = PACK_ARGB4444(a, r, g, b);
    } else {
        assert(format == ARGB1555);

        GLushort* s1 = (GLushort*) src1;
        GLushort* s2 = (GLushort*) src2;
        GLushort* s3 = (GLushort*) src3;
        GLushort* s4 = (GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        a = A1555(*s1) + A1555(*s2) + A1555(*s3) + A1555(*s4);
        r = R1555(*s1) + R1555(*s2) + R1555(*s3) + R1555(*s4);
        g = G1555(*s1) + G1555(*s2) + G1555(*s3) + G1555(*s4);
        b = B1555(*s1) + B1555(*s2) + B1555(*s3) + B1555(*s4);

        a /= 4;
        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = PACK_ARGB1555((GLubyte) a, (GLubyte) r, (GLubyte) g, (GLubyte) b);
    }

    return GL_TRUE;
}

GLboolean _glGenerateMipmapTwiddled(const GLuint pvrFormat, const GLubyte* prevData, GLuint thisWidth, GLuint thisHeight, GLubyte* thisData) {
    uint32_t lastWidth = thisWidth * 2;
    uint32_t lastHeight = thisHeight * 2;

    uint32_t i, j;
    uint32_t stride = 0;

    if((pvrFormat & GPU_TXRFMT_PAL8BPP) == GPU_TXRFMT_PAL8BPP) {
        stride = 1;
    } else {
        stride = 2;
    }

    for(i = 0, j = 0; i < lastWidth * lastHeight; i += 4, j++) {

        /* In a twiddled texture, the neighbouring texels
         * are next to each other. By averaging them we just basically shrink
         * the reverse Ns so each reverse N becomes the next level down... if that makes sense!? */

        const GLubyte* s1 = &prevData[i * stride];
        const GLubyte* s2 = s1 + stride;
        const GLubyte* s3 = s2 + stride;
        const GLubyte* s4 = s3 + stride;
        GLubyte* t = &thisData[j * stride];

        assert(s4 < prevData + (lastHeight * lastWidth * stride));
        assert(t < thisData + (thisHeight * thisWidth * stride));

        _glCalculateAverageTexel(pvrFormat, s1, s2, s3, s4, t);
    }

    return GL_TRUE;
}

void APIENTRY glGenerateMipmapEXT(GLenum target) {
    if(target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    TextureObject* tex = _glGetBoundTexture();

    if(!tex || !tex->data || !tex->mipmapCount) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    if(tex->width != tex->height) {
        fprintf(stderr, "[GL ERROR] Mipmaps cannot be supported on non-square textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    if((tex->color & GPU_TXRFMT_NONTWIDDLED) == GPU_TXRFMT_NONTWIDDLED) {
        /* glTexImage2D should twiddle internally textures in nearly all cases
         * so this error is unlikely */

        fprintf(stderr, "[GL ERROR] Mipmaps are only supported on twiddled textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    GLboolean complete = _glIsMipmapComplete(tex);
    if(!complete && tex->isCompressed) {
        fprintf(stderr, "[GL ERROR] Generating mipmaps for compressed textures is not yet supported\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        _glKosPrintError();
        return;
    }

    if(complete) {
        /* Nothing to do */
        return;
    }

    GLuint i;
    GLuint prevWidth = tex->width;
    GLuint prevHeight = tex->height;

    /* Make sure there is room for the mipmap data on the texture object */
    _glAllocateSpaceForMipmaps(tex);

    for(i = 1; i < _glGetMipmapLevelCount(tex); ++i) {
        GLubyte* prevData = _glGetMipmapLocation(tex, i - 1);
        GLubyte* thisData = _glGetMipmapLocation(tex, i);

        GLuint thisWidth = (prevWidth > 1) ? prevWidth / 2 : 1;
        GLuint thisHeight = (prevHeight > 1) ? prevHeight / 2 : 1;

        _glGenerateMipmapTwiddled(tex->color, prevData, thisWidth, thisHeight, thisData);

        tex->mipmap |= (1 << i);

        prevWidth = thisWidth;
        prevHeight = thisHeight;
    }

    assert(_glIsMipmapComplete(tex));
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
