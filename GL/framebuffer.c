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

void APIENTRY glGenerateMipmapEXT(GLenum target) {

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
