/* KallistiGL for KallistiOS ##version##

   libgl/glu.h
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Some functionality adapted from the original KOS libgl:
   Copyright (C) 2001 Dan Potter
   Copyright (C) 2002 Benoit Miller

*/

#ifndef __GL_GLU_H
#define __GL_GLU_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <GL/gl.h>

#define GLU_FALSE GL_FALSE
#define GLU_TRUE GL_TRUE

/* Mip-Mapped Textures MUST be square or rectangle */
GLAPI GLint APIENTRY gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                                       GLsizei width, GLsizei height,
                                       GLenum format, GLenum type,
                                       const void *data);

/* gluPerspective - Set the Perspective for Rendering. */
GLAPI void APIENTRY gluPerspective(GLdouble fovy, GLdouble aspect,
                                   GLdouble zNear, GLdouble zFar);

/* gluLookAt - Set Camera Position for Rendering. */
GLAPI void APIENTRY gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez,
                              GLfloat centerx, GLfloat centery, GLfloat centerz,
                              GLfloat upx, GLfloat upy, GLfloat upz);
                              
/* glhLookAtf2 = gluLookAt operating on 3 float vectors. */
GLAPI void APIENTRY glhLookAtf2(GLfloat *eyePosition3D,
                                GLfloat *center3D,
                                GLfloat *upVector3D);

__END_DECLS

#endif /* !__GL_GLU_H */
