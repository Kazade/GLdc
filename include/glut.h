/* KallistiGL for KallistiOS ##version##

   libgl/glut.h
   Copyright (C) 2014 Josh Pearson
   Copyright (C) 2014 Lawrence Sebald

*/

#ifndef __GL_GLUT_H
#define __GL_GLUT_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <GL/gl.h>

/* Flush the Submitted Primitive Data to the GPU for render */
GLAPI void APIENTRY glutSwapBuffers();

/* Copy the Submitted Primitive Data to the GPU for render to texture */
/* This will leave the Vertex Data in the Main Buffer to be Flushed on the
   next frame rendered */
GLAPI void APIENTRY glutCopyBufferToTexture(void *dst, GLsizei *x, GLsizei *y);

__END_DECLS

#endif /* !__GL_GLUT_H */
