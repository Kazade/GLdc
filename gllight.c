/* KallistiGL for KOS ##version##

   gllight.c
   (c)2001 Dan Potter
*/

#include <GL/gl.h>
#include <GL/glu.h>
#include "glinternal.h"
#include "glmacros.h"

#include "assert.h"

/* Lighting */
void glShadeModel(GLenum mode) {
	assert_msg(!gl_prim_type, "Not allowed within glBegin/glEnd pair.");
	switch(mode)
	{
	case GL_FLAT:
		gl_poly_cxt.gen.shading = PVR_SHADE_FLAT;
		break;
	case GL_SMOOTH:
		gl_poly_cxt.gen.shading = PVR_SHADE_GOURAUD;
		break;
	}
}

