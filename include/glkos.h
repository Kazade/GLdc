#ifndef GLKOS_H
#define GLKOS_H

#include "gl.h"

__BEGIN_DECLS

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
