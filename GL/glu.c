#include <math.h>
#include "private.h"

/* Set the Perspective */
void gluPerspective(GLfloat angle, GLfloat aspect,
                    GLfloat znear, GLfloat zfar) {
    GLfloat xmin, xmax, ymin, ymax;

    ymax = znear * tanf(angle * F_PI / 360.0f);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustum(xmin, xmax, ymin, ymax, znear, zfar);
}
