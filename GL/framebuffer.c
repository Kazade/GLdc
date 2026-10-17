#include <stdio.h>
#include <string.h>

#include "private.h"

#ifdef _arch_dreamcast
#include <arch/cache.h>
#endif

typedef struct {
    GLuint index;
    GLuint texture_id;
    GLboolean is_complete;
    GLboolean dirty;

    /* FIXME: Add OP, TR and PT lists per framebuffer */

} FrameBuffer;

static FrameBuffer* ACTIVE_FRAMEBUFFER = NULL;
static NamedArray FRAMEBUFFERS;
static GLboolean FBO_SCENE_OPEN = GL_FALSE;
static GLboolean FBO_SCENE_IS_RTT = GL_FALSE;

GLboolean _glFramebufferBeginScene(void) {
#ifdef _arch_dreamcast
    if(!FBO_SCENE_OPEN) {
        TextureObject* txr = NULL;

        if(ACTIVE_FRAMEBUFFER && ACTIVE_FRAMEBUFFER->texture_id) {
            txr = _glGetTexture(ACTIVE_FRAMEBUFFER->texture_id);
        }

        if(txr && txr->isRTT && txr->data) {
#if GLDC_HAVE_PVR_SCENE_BEGIN_RTT
            uint32_t render_w = txr->logicalWidth;
            uint32_t render_h = txr->logicalHeight;
            uint32_t stride_px = txr->rttWidth;

            if(GLDC_RTT_TRACE) {
                fprintf(stderr,
                        "[rtt] bind fbo=%u tex=%u begin txr_ex ptr=%p render=%ux%u stride=%u backing=%ux%u logical=%ux%u\n",
                        (unsigned)(ACTIVE_FRAMEBUFFER ? ACTIVE_FRAMEBUFFER->index : 0),
                        (unsigned)txr->index,
                        txr->data,
                        (unsigned)render_w,
                        (unsigned)render_h,
                        (unsigned)stride_px,
                        (unsigned)txr->rttWidth,
                        (unsigned)txr->rttHeight,
                        (unsigned)txr->logicalWidth,
                        (unsigned)txr->logicalHeight);
            }

            if(SceneBeginRTT(txr->data, render_w, render_h, stride_px) < 0) {
                if(GLDC_RTT_TRACE) {
                    fprintf(stderr, "[rtt] begin txr_ex failed\n");
                }
                FBO_SCENE_IS_RTT = GL_FALSE;
                return GL_FALSE;
            }
            FBO_SCENE_IS_RTT = GL_TRUE;
#else
            if(GLDC_RTT_TRACE) {
                fprintf(stderr, "[rtt] unavailable: KOS lacks pvr_scene_begin_rtt\n");
            }
            FBO_SCENE_IS_RTT = GL_FALSE;
            return GL_FALSE;
#endif
        } else {
            SceneBegin();
            FBO_SCENE_IS_RTT = GL_FALSE;
        }
        FBO_SCENE_OPEN = GL_TRUE;
        return GL_TRUE;
    }
#endif
    return GL_TRUE;
}

void _glFramebufferFinishScene(void) {
#ifdef _arch_dreamcast
    if(FBO_SCENE_OPEN) {
        if(FBO_SCENE_IS_RTT) {
            if(GLDC_RTT_TRACE) {
                fprintf(stderr, "[rtt] finish txr sync_mode=%d\n", GLDC_RTT_SYNC_MODE);
            }
            pvr_scene_finish();
            if(GLDC_RTT_SYNC_MODE >= 1) {
                pvr_wait_ready();
            }
            if(GLDC_RTT_SYNC_MODE >= 2) {
                pvr_wait_render_done();
            }
            __asm__ __volatile__("" ::: "memory");
            (void)(*(volatile uint32_t *)0xa05f8000);
        } else {
            SceneFinish();
            pvr_wait_ready();
            pvr_wait_render_done();
        }
        FBO_SCENE_OPEN = GL_FALSE;
        FBO_SCENE_IS_RTT = GL_FALSE;
    }
#endif
}

void _glFramebufferSceneClosed(void) {
#ifdef _arch_dreamcast
    FBO_SCENE_OPEN = GL_FALSE;
#endif
}

static FrameBuffer* _glGetFramebuffer(GLuint id) {
    return (FrameBuffer*) named_array_get(&FRAMEBUFFERS, id);
}

static GLenum _glFramebufferStatus(FrameBuffer* fb) {
    if(!fb) {
        return GL_FRAMEBUFFER_COMPLETE_EXT;
    }

    if(!fb->texture_id) {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT;
    }

    TextureObject* txr = _glGetTexture(fb->texture_id);
    if(!txr || !txr->data) {
        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT;
    }

#ifdef _arch_dreamcast
#if !GLDC_HAVE_PVR_SCENE_BEGIN_RTT
    return GL_FRAMEBUFFER_UNSUPPORTED_EXT;
#else
    if(!txr->isRTT || !txr->rttWidth || !txr->rttHeight) {
        return GL_FRAMEBUFFER_UNSUPPORTED_EXT;
    }
#endif
#endif

    // if(txr->internalFormat != GL_RGB565_KOS &&
    //    txr->internalFormat != GL_ARGB1555_KOS) {
    //     return GL_FRAMEBUFFER_UNSUPPORTED_EXT;
    // }

    return GL_FRAMEBUFFER_COMPLETE_EXT;
}

static void _glResolveFramebufferToTexture(FrameBuffer* fb) {
    _GL_UNUSED(fb);
}

void _glInitFramebuffers() {
    named_array_init(&FRAMEBUFFERS, sizeof(FrameBuffer), 32);

    // Reserve zero so that it is never given to anyone as an ID!
    named_array_reserve(&FRAMEBUFFERS, 0);
}

void _glWipeTextureOnFramebuffers(GLuint texture) {
    for(GLuint id = 0; id < FRAMEBUFFERS.max_element_count; ++id) {
        if(!named_array_used(&FRAMEBUFFERS, id)) {
            continue;
        }

        FrameBuffer* fb = _glGetFramebuffer(id);
        if(fb && fb->texture_id == texture) {
            fb->texture_id = 0;
            fb->dirty = GL_FALSE;
            fb->is_complete = GL_FALSE;
        }
    }
}

void _glFramebufferMarkDirty(void) {
    if(!ACTIVE_FRAMEBUFFER) {
        /*
         * Default framebuffer rendering still uses the queued polygon lists.
         * Non-FBO samples must be able to start a scene from glClear/draw
         * instead of relying on a prior fake-FBO unbind.
         */
        (void)_glBeginQueuedScene();
        return;
    }

    TextureObject* txr = ACTIVE_FRAMEBUFFER->texture_id ? _glGetTexture(ACTIVE_FRAMEBUFFER->texture_id) : NULL;
    if(!txr || !txr->isRTT || !txr->data) {
        return;
    }

    _glBeginQueuedScene();

    ACTIVE_FRAMEBUFFER->dirty = GL_TRUE;
}

void _glResolveActiveFramebuffer(void) {
    _glResolveFramebufferToTexture(ACTIVE_FRAMEBUFFER);
}

void _glGetFramebufferRenderSize(GLsizei* width, GLsizei* height) {
    if(ACTIVE_FRAMEBUFFER && ACTIVE_FRAMEBUFFER->texture_id) {
        TextureObject* txr = _glGetTexture(ACTIVE_FRAMEBUFFER->texture_id);
        if(txr && txr->isRTT && txr->logicalWidth && txr->logicalHeight) {
            if(width) {
                *width = (GLsizei)txr->logicalWidth;
            }
            if(height) {
                *height = (GLsizei)txr->logicalHeight;
            }
            return;
        }
    }

    const VideoMode* mode = GetVideoMode();
    if(width) {
        *width = (GLsizei)mode->width;
    }
    if(height) {
        *height = (GLsizei)mode->height;
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
        fb->dirty = GL_FALSE;

        *framebuffers = id;
        framebuffers++;
    }
}

void APIENTRY glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) {
    TRACE();

    while(n--) {
        FrameBuffer* fb = _glGetFramebuffer(*framebuffers);

        if(!fb) {
            framebuffers++;
            continue;
        }

        if(fb == ACTIVE_FRAMEBUFFER) {
            ACTIVE_FRAMEBUFFER = NULL;
        }

        named_array_release(&FRAMEBUFFERS, *framebuffers++);
    }
}

void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer) {
    TRACE();

    if(target != GL_FRAMEBUFFER_EXT) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    FrameBuffer* next = NULL;

    if(framebuffer) {
        next = _glGetFramebuffer(framebuffer);
        if(!next) {
            _glKosThrowError(GL_INVALID_OPERATION, __func__);
            return;
        }
    }

    if(next == ACTIVE_FRAMEBUFFER) {
        return;
    }

    if(ACTIVE_FRAMEBUFFER) {
        if(GLDC_RTT_TRACE) {
            TextureObject* txr = ACTIVE_FRAMEBUFFER->texture_id ? _glGetTexture(ACTIVE_FRAMEBUFFER->texture_id) : NULL;
            if(txr && txr->isRTT) {
                fprintf(stderr, "[rtt] unbind fbo=%u finish txr\n", (unsigned)ACTIVE_FRAMEBUFFER->index);
            } else {
                fprintf(stderr, "[rtt] unbind fbo=%u finish scene\n", (unsigned)ACTIVE_FRAMEBUFFER->index);
            }
        }
        _glFinishQueuedScene();
    }

    ACTIVE_FRAMEBUFFER = next;
    _glGPUStateMarkDirty();
}

void APIENTRY glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if(target != GL_FRAMEBUFFER_EXT) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    if(attachment != GL_COLOR_ATTACHMENT0_EXT) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    if(textarget != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    if(level != 0) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    if(texture != 0 && !glIsTexture(texture)) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(!ACTIVE_FRAMEBUFFER) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(ACTIVE_FRAMEBUFFER->texture_id != texture && (ACTIVE_FRAMEBUFFER->dirty || FBO_SCENE_OPEN)) {
        /*
         * Preserve the contents rendered into the previously attached texture
         * before replacing or detaching it. Otherwise changing attachments on a
         * dirty fake FBO silently drops the pending render.
         */
        _glFinishQueuedScene();
    }

    ACTIVE_FRAMEBUFFER->texture_id = texture;
    ACTIVE_FRAMEBUFFER->dirty = GL_FALSE;

    if(texture) {
        TextureObject* txr = _glGetTexture(texture);
        if(txr) {
            _glPromoteTextureToRTT(txr);
        }
    }

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

    const GLuint format = pvrFormat & (7u << 27);

    if(format == GPU_TXRFMT_PAL8BPP) {
        /* Paletted... all we can do really is just pick one of the
         * 4 texels.. unless we want to change the palette (bad) or
         * pick the closest available colour (slow, and probably bad)
         */
        *t = *src1;
    } else if(format == GPU_TXRFMT_RGB565) {
        const GLushort* s1 = (const GLushort*) src1;
        const GLushort* s2 = (const GLushort*) src2;
        const GLushort* s3 = (const GLushort*) src3;
        const GLushort* s4 = (const GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        r = R565(*s1) + R565(*s2) + R565(*s3) + R565(*s4);
        g = G565(*s1) + G565(*s2) + G565(*s3) + G565(*s4);
        b = B565(*s1) + B565(*s2) + B565(*s3) + B565(*s4);

        r /= 4;
        g /= 4;
        b /= 4;

        *d1 = PACK_RGB565(r, g, b);
    } else if(format == GPU_TXRFMT_ARGB4444) {
        const GLushort* s1 = (const GLushort*) src1;
        const GLushort* s2 = (const GLushort*) src2;
        const GLushort* s3 = (const GLushort*) src3;
        const GLushort* s4 = (const GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        a = A4444(*s1) + A4444(*s2) + A4444(*s3) + A4444(*s4);
        r = R4444(*s1) + R4444(*s2) + R4444(*s3) + R4444(*s4);
        g = G4444(*s1) + G4444(*s2) + G4444(*s3) + G4444(*s4);
        b = B4444(*s1) + B4444(*s2) + B4444(*s3) + B4444(*s4);

        a /= 4;
        r /= 4;
        g /= 4;
        b /= 4;

        /* These values are already 4-bit channels; PACK_ARGB4444 expects
         * 8-bit inputs and would quantize them a second time. */
        *d1 = (GLushort)((a << 12) | (r << 8) | (g << 4) | b);
    } else if(format == GPU_TXRFMT_ARGB1555) {

        const GLushort* s1 = (const GLushort*) src1;
        const GLushort* s2 = (const GLushort*) src2;
        const GLushort* s3 = (const GLushort*) src3;
        const GLushort* s4 = (const GLushort*) src4;
        GLushort* d1 = (GLushort*) t;

        const GLuint opaqueCount =
            ((*s1 >> 15) & 1u) +
            ((*s2 >> 15) & 1u) +
            ((*s3 >> 15) & 1u) +
            ((*s4 >> 15) & 1u);
        r = R1555(*s1) + R1555(*s2) + R1555(*s3) + R1555(*s4);
        g = G1555(*s1) + G1555(*s2) + G1555(*s3) + G1555(*s4);
        b = B1555(*s1) + B1555(*s2) + B1555(*s3) + B1555(*s4);

        r /= 4;
        g /= 4;
        b /= 4;

        /* Preserve thin punch-through features when a 2x2 block is exactly
         * half covered. A strict majority erodes one-texel fence lines when
         * hardware LOD selection reaches the next smaller level. */
        a = opaqueCount >= 2u ? 255u : 0u;
        *d1 = PACK_ARGB1555(a, r, g, b);
    } else {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return GL_FALSE;
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

        gl_assert(s4 < prevData + (lastHeight * lastWidth * stride));
        gl_assert(t < thisData + (thisHeight * thisWidth * stride));

        _glCalculateAverageTexel(pvrFormat, s1, s2, s3, s4, t);
    }

    return GL_TRUE;
}

void APIENTRY glGenerateMipmap(GLenum target) {
    if(target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    TextureObject* tex = _glGetBoundTexture();

    if(!tex || !tex->data || !tex->mipmapCount) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(tex->width != tex->height) {
        fprintf(stderr, "[GL ERROR] Mipmaps cannot be supported on non-square textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if((tex->color & GPU_TXRFMT_PAL4BPP) == GPU_TXRFMT_PAL4BPP) {
        fprintf(stderr, "[GL ERROR] Mipmap generation not supported for 4BPP paletted textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if((tex->color & GPU_TXRFMT_NONTWIDDLED) == GPU_TXRFMT_NONTWIDDLED) {
        /* glTexImage2D should twiddle internally textures in nearly all cases
         * so this error is unlikely */

        fprintf(stderr, "[GL ERROR] Mipmaps are only supported on twiddled textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    GLboolean complete = _glIsMipmapComplete(tex);
    if(!complete && tex->isCompressed) {
        fprintf(stderr, "[GL ERROR] Generating mipmaps for compressed textures is not yet supported\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(complete) {
        /* Nothing to do */
        return;
    }

    GLuint i;
    GLuint prevWidth = tex->width;
    GLuint prevHeight = tex->height;

    /* Keep mip generation in system RAM. Texture allocations use the PVR's
     * 64-bit VRAM mapping on Dreamcast; ordinary CPU writes to that mapping
     * are not a reliable texture upload mechanism on real hardware. */
    GLubyte* baseData = (GLubyte*) malloc(tex->baseDataSize);
    if(!baseData) {
        _glKosThrowError(GL_OUT_OF_MEMORY, __func__);
        return;
    }
    const GLubyte* currentBase = tex->baseDataOffset
        ? _glGetMipmapLocation(tex, 0)
        : (const GLubyte*)tex->data;
    GPUTextureRead(currentBase, baseData, tex->baseDataSize);

    /* Make sure there is room for the mipmap data on the texture object */
    _glAllocateSpaceForMipmaps(tex);
    if(!tex->data) {
        free(baseData);
        return;
    }

    const GLuint dataBytes = tex->baseDataOffset + tex->baseDataSize;
    const GLuint uploadBytes = (dataBytes + 31u) & ~31u;
    GLubyte* mipData = (GLubyte*) calloc(1, uploadBytes);
    if(!mipData) {
        free(baseData);
        _glKosThrowError(GL_OUT_OF_MEMORY, __func__);
        return;
    }
    memcpy(mipData + tex->baseDataOffset, baseData, tex->baseDataSize);
    free(baseData);

    for(i = 1; i < _glGetMipmapLevelCount(tex); ++i) {
        const size_t prevOffset = (size_t)(_glGetMipmapLocation(tex, i - 1) - (GLubyte*)tex->data);
        const size_t thisOffset = (size_t)(_glGetMipmapLocation(tex, i) - (GLubyte*)tex->data);
        GLubyte* prevData = mipData + prevOffset;
        GLubyte* thisData = mipData + thisOffset;

        GLuint thisWidth = (prevWidth > 1) ? prevWidth / 2 : 1;
        GLuint thisHeight = (prevHeight > 1) ? prevHeight / 2 : 1;

        _glGenerateMipmapTwiddled(tex->color, prevData, thisWidth, thisHeight, thisData);

        tex->mipmap |= (1 << i);

        prevWidth = thisWidth;
        prevHeight = thisHeight;
    }

    GPUTextureLoad(mipData, tex->data, uploadBytes);
    free(mipData);

    gl_assert(_glIsMipmapComplete(tex));
}

GLenum APIENTRY glCheckFramebufferStatusEXT(GLenum target) {
    if(target != GL_FRAMEBUFFER_EXT) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return 0;
    }

    if(!ACTIVE_FRAMEBUFFER) {
        return GL_FRAMEBUFFER_COMPLETE_EXT;
    }

    GLenum status = _glFramebufferStatus(ACTIVE_FRAMEBUFFER);
    ACTIVE_FRAMEBUFFER->is_complete = (status == GL_FRAMEBUFFER_COMPLETE_EXT) ? GL_TRUE : GL_FALSE;
    return status;
}

GLboolean APIENTRY glIsFramebufferEXT(GLuint framebuffer) {
    return (named_array_used(&FRAMEBUFFERS, framebuffer)) ? GL_TRUE : GL_FALSE;
}
