#include "gl.h"
#include "glu.h"
#include "glkos.h"
#include  "gl_png.h"

//$KOS_BASE/utils/texconv/texconv  --in disk.png --format ARGB4444 --preview disk_preview.png --out disk.dtex

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
texture t;
int 	blendActive = -1;
/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix
    glOrtho(-3, 3, -3, 3, -10, 10);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0)				// Prevent A Divide By Zero If The Window Is Too Small
        Height = 1;

    glViewport(0, 0, Width, Height);		// Reset The Current Viewport And Perspective Transformation

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(-3, 3, -3, 3, -10, 10);

    glMatrixMode(GL_MODELVIEW);
}


void DrawSquare(float width, float r, float g, float b, float z) {
    width /= 2;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);				// start drawing a polygon (4 sided)
    glVertex3f(-width, width, z);		// Top Left
    glVertex3f( width, width, z);		// Top Right
    glVertex3f( width,-width, z);		// Bottom Right
    glVertex3f(-width,-width, z);		// Bottom Left
    glEnd();					// done with the polygon
}

/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View


    //First Batch is alpha blending
    glTranslated(-1 ,0, -5);
    for (int i = 0; i < 5; i++) {
      glTranslated(0.5, 0, 0);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
      draw_textured_quad(&t);
      glDisable(GL_BLEND);
    }


    //Second batch is depth testing
    //Changing the translate Z value doesn't change anything?
    glLoadIdentity();
    glTranslated(-1 , -1, -5);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_FUNC);

    for (int i = 0; i < 5; i++) {
      glTranslated(0.5, 0, -0.2);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
      draw_textured_quad(&t);
      glDisable(GL_BLEND);
    }

    glDisable(GL_DEPTH_TEST);

    // swap buffers to display, since we're double buffered.
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();
    InitGL(640, 480);

    //loads a dtex texture. see the /romdisk folder for more files
    dtex_to_gl_texture(&t, "/rd/disk_1555.dtex");
    ReSizeGLScene(640, 480);
    DrawGLScene();
    while(1) {
      DrawGLScene();
    }

    return 0;
}
