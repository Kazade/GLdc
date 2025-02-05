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
