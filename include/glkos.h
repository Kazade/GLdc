#ifndef GLKOS_H
#define GLKOS_H

#include "gl.h"

__BEGIN_DECLS


#define GL_NEARZ_CLIPPING_KOS                 0xEEFA

#define GL_COLOR_ATTACHMENT0_EXT              0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT              0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT              0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT              0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT              0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT              0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT              0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT              0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT              0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT              0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT             0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT             0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT             0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT             0x8CED
#define GL_COLOR_ATTACHMENT14_EXT             0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT             0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT               0x8D00
#define GL_STENCIL_ATTACHMENT_EXT             0x8D20
#define GL_FRAMEBUFFER_EXT                    0x8D40
#define GL_RENDERBUFFER_EXT                   0x8D41
#define GL_RENDERBUFFER_WIDTH_EXT            0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT            0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT   0x8D44

#define GL_FRAMEBUFFER_COMPLETE_EXT                      0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT         0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT        0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT        0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT                   0x8CDD

GLAPI void APIENTRY glKosSwapBuffers();

GLAPI void APIENTRY glGenFramebuffersEXT(GLsizei n, GLuint* framebuffers);
GLAPI void APIENTRY glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers);
GLAPI void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer);
GLAPI void APIENTRY glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLAPI void APIENTRY glGenerateMipmapEXT(GLenum target);
GLAPI GLenum APIENTRY glCheckFramebufferStatusEXT(GLenum target);
GLAPI GLboolean APIENTRY glIsFramebufferEXT(GLuint framebuffer);

__END_DECLS

#endif // GLKOS_H
