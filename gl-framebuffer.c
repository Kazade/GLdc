/* KallistiGL for KallistiOS ##version##

   libgl/gl-framebuffer.c
   Copyright (C) 2014 Josh Pearson

   This file implements Open GL Frame Buffer Object (FBO) functionality, with what
   the DC's PVR can directly implement.
   Basically, Render-To-Texture using GL_RGB565 is the only  native feature of the
   PVR, so if you are looking for a depth-buffer, bad news.

   This implementation uses a dynamic linked list to implement the data structures needed.
*/

#include "gl.h"
#include "glext.h"
#include "gl-api.h"

#include <malloc.h>

//========================================================================================//
//== Internal KOS Open GL API FBO Structures / Global Variables ==//

static GL_FRAMEBUFFER_OBJECT *FRAMEBUF_OBJ    = NULL;
static GLsizei                 FRAMEBUF_OBJECT = 0;

//========================================================================================//
//== Internal KOS Open GL API FBO Functionality ==//

void _glKosInitFrameBuffers() {
    FRAMEBUF_OBJ = malloc(sizeof(GL_FRAMEBUFFER_OBJECT));

    FRAMEBUF_OBJ->index = 0;
    FRAMEBUF_OBJ->texID = 0;
    FRAMEBUF_OBJ->data = NULL;
    FRAMEBUF_OBJ->link = NULL;
}

static void _glKosInsertFramebufferObj(GL_FRAMEBUFFER_OBJECT *obj) {
    GL_FRAMEBUFFER_OBJECT *ptr = FRAMEBUF_OBJ;

    while(ptr->link != NULL)
        ptr = (GL_FRAMEBUFFER_OBJECT *)ptr->link;

    ptr->link = obj;
}

static GLsizei _glKosGetLastFrameBufferIndex() {
    GL_FRAMEBUFFER_OBJECT *ptr = FRAMEBUF_OBJ;

    while(ptr->link != NULL)
        ptr = (GL_FRAMEBUFFER_OBJECT *)ptr->link;

    return ptr->index;
}

static GL_FRAMEBUFFER_OBJECT *_glKosGetFrameBufferObj(GLuint index) {
    GL_FRAMEBUFFER_OBJECT *ptr = FRAMEBUF_OBJ;

    while(ptr->index != index && ptr->link != NULL)
        ptr = (GL_FRAMEBUFFER_OBJECT *)ptr->link;

    return ptr;
}

GLsizei _glKosGetFBO() {
    return FRAMEBUF_OBJECT;
}

GLuint _glKosGetFBOWidth(GLsizei fbi) {
    GL_FRAMEBUFFER_OBJECT *fbo = _glKosGetFrameBufferObj(fbi);
    return _glKosTextureWidth(fbo->texID);
}

GLuint _glKosGetFBOHeight(GLsizei fbi) {
    GL_FRAMEBUFFER_OBJECT *fbo = _glKosGetFrameBufferObj(fbi);
    return _glKosTextureHeight(fbo->texID);
}

GLvoid *_glKosGetFBOData(GLsizei fbi) {
    GL_FRAMEBUFFER_OBJECT *fbo = _glKosGetFrameBufferObj(fbi);
    return fbo->data;
}

//========================================================================================//
//== Public KOS Open GL API FBO Functionality ==//

GLAPI void APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    GLsizei index = _glKosGetLastFrameBufferIndex();

    while(n--) {
        GL_FRAMEBUFFER_OBJECT *obj = malloc(sizeof(GL_FRAMEBUFFER_OBJECT));
        obj->index = ++index;
        obj->texID = 0;
        obj->data = NULL;
        obj->link = NULL;

        _glKosInsertFramebufferObj(obj);

        *framebuffers++ = obj->index;
    }
}

GLAPI void APIENTRY glDeleteFramebuffers(GLsizei n, GLuint *framebuffers) {
    while(n--) {
        GL_FRAMEBUFFER_OBJECT *ptr = FRAMEBUF_OBJ->link, * lptr = FRAMEBUF_OBJ;

        while(ptr != NULL) {
            if(ptr->index == *framebuffers) {
                GL_FRAMEBUFFER_OBJECT *cur_node = ptr;
                lptr->link = ptr->link;
                ptr = (GL_FRAMEBUFFER_OBJECT *)ptr->link;
                free(cur_node);

                if(*framebuffers == FRAMEBUF_OBJECT)
                    FRAMEBUF_OBJECT = 0;

                break;
            }
            else
                ptr = (GL_FRAMEBUFFER_OBJECT *)ptr->link;
        }

        ++framebuffers;
    }
}

GLAPI void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if(target != GL_FRAMEBUFFER) {
        _glKosThrowError(GL_INVALID_ENUM, "glBindFramebuffer");
        _glKosPrintError();
        return;
    }

    FRAMEBUF_OBJECT = framebuffer;
}

GLAPI void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level) {
    if(target != GL_FRAMEBUFFER)
        _glKosThrowError(GL_INVALID_ENUM, "glFramebufferTexture2D");

    if(attachment != GL_COLOR_ATTACHMENT0)
        _glKosThrowError(GL_INVALID_OPERATION, "glFramebufferTexture2D");

    if(textarget != GL_TEXTURE_2D)
        _glKosThrowError(GL_INVALID_OPERATION, "glFramebufferTexture2D");

    if(level)
        _glKosThrowError(GL_INVALID_ENUM, "glFramebufferTexture2D");

    if(!FRAMEBUF_OBJECT)
        _glKosThrowError(GL_INVALID_OPERATION, "glFramebufferTexture2D");

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    GL_FRAMEBUFFER_OBJECT *fbo = _glKosGetFrameBufferObj(FRAMEBUF_OBJECT);

    fbo->texID = texture;
    fbo->data = _glKosTextureData(texture);
}

GLAPI GLenum APIENTRY glCheckFramebufferStatus(GLenum target) {
    if(target != GL_FRAMEBUFFER) {
        _glKosThrowError(GL_INVALID_ENUM, "glCheckFramebufferStatus");
        _glKosPrintError();
        return 0;
    }

    if(!FRAMEBUF_OBJECT)
        return GL_FRAMEBUFFER_COMPLETE;

    GL_FRAMEBUFFER_OBJECT *fbo = _glKosGetFrameBufferObj(FRAMEBUF_OBJECT);

    if(!fbo->texID)
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;

    return GL_FRAMEBUFFER_COMPLETE;
}
