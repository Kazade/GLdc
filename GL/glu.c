#include <math.h>
#include "private.h"

/* Set the Perspective */
void APIENTRY gluPerspective(GLdouble angle, GLdouble aspect,
                    GLdouble znear, GLdouble zfar) {
    GLdouble fW, fH;

    fH = tan(angle * (M_PI / 360.0)) * znear;
    fW = fH * aspect;

    glFrustum(-fW, fW, -fH, fH, znear, zfar);
}

void APIENTRY gluOrtho2D(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top) {
    glOrtho(left, right, bottom, top, -1.0f, 1.0f);
}

/* generate mipmaps for any image provided by the user and then pass them to OpenGL */
GLint APIENTRY gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                                       GLsizei width, GLsizei height,
                                       GLenum format, GLenum type, const void *data){
    /* 2d texture, level of detail 0 (normal), 3 components (red, green, blue),
	 width & height of the image, border 0 (normal), rgb color data,
	 unsigned byte data, and finally the data itself. */
    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);
    // FIXME return glu error codes
    return 0;
}
