/*
   KallistiOS 2.0.0

   main.c
   (c)2014 Josh Pearson

   Open GL Multi-Texture example using Vertex Array Submission.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"
#include "GL/glext.h"

#ifdef __DREAMCAST__
#define IMAGE1_FILENAME "/rd/wp001vq.pvr"
#define IMAGE2_FILENAME "/rd/FlareWS_256.pvr"
#else
#define IMAGE1_FILENAME "samples/multitexture_arrays/romdisk/wp001vq.pvr"
#define IMAGE2_FILENAME "samples/multitexture_arrays/romdisk/FlareWS_256.pvr"
#endif

/* Load a PVR texture - located in pvr-texture.c */
extern GLuint glTextureLoadPVR(char *fname, unsigned char isMipMapped, unsigned char glMipMap);

GLfloat VERTEX_ARRAY[4 * 3] = { -1.0f,  1.0f, 0.0f,
                                1.0f,  1.0f, 0.0f,
                                1.0f, -1.0f, 0.0f,
                                -1.0f, -1.0f, 0.0f
                              };

GLfloat TEXCOORD_ARRAY[4 * 2] = { 0, 0,
                                  1, 0,
                                  1, 1,
                                  0, 1
                                };

GLuint ARGB_ARRAY[4] = { 0xFFFF0000, 0xFF0000FF, 0xFF00FF00, 0xFFFFFF00 };


/* Multi-Texture Example using Open GL Vertex Buffer Submission. */
void RenderCallback(GLuint texID0, GLuint texID1) {
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -3.0f);

    /* Enable Client States for OpenGL Arrays Submission */
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    /* Bind texture to GL_TEXTURE0_ARB and set texture parameters */
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texID0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    /* Bind multi-texture to GL_TEXTURE1_ARB and set texture parameters */
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texID1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* Set Blending Mode */
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);

    /* Bind texture coordinates to GL_TEXTURE0_ARB */
    glTexCoordPointer(2, GL_FLOAT, 0, TEXCOORD_ARRAY);

    /* Bind texture coordinates to GL_TEXTURE1_ARB */
    glClientActiveTextureARB(GL_TEXTURE1_ARB);
    glTexCoordPointer(2, GL_FLOAT, 0, TEXCOORD_ARRAY);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);

    /* Bind the Color Array */
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, 0, ARGB_ARRAY);

    /* Bind the Vertex Array */
    glVertexPointer(3, GL_FLOAT, 0, VERTEX_ARRAY);

    /* Render the Vertices as Indexed Arrays using glDrawArrays */
    glDrawArrays(GL_QUADS, 0, 4);

    /* Disable GL_TEXTURE1 */
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    /* Make sure to set glActiveTexture back to GL_TEXTURE0_ARB when finished */
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    /* Disable Vertex, Color and Texture Coord Arrays */
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

extern uint8_t romdisk[];
KOS_INIT_ROMDISK(romdisk);

int main(int argc, char **argv) {
    /* Notice we do not init the PVR here, that is handled by Open GL */
    glKosInit();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Load two PVR textures to OpenGL */
    GLuint texID0 = glTextureLoadPVR(IMAGE1_FILENAME, 0, 0);
    GLuint texID1 = glTextureLoadPVR(IMAGE2_FILENAME, 0, 0);

    while(1) {
        /* Draw the "scene" */
        RenderCallback(texID0, texID1);

        /* Finish the frame - Notice there is no glKosBegin/FinshFrame */
        glKosSwapBuffers();
    }

    return 0;
}

