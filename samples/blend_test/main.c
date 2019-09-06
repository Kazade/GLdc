/*
 * This sample is to demonstrate a bug where rendering an unblended
 * polygon, before a series of blended ones would result in no blended
 * output and incorrect depth testing
 */

#include "gl.h"
#include "glu.h"
#include "glkos.h"

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

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

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);
    glMatrixMode(GL_MODELVIEW);
}


void DrawQuad(const float* colour) {
    glBegin(GL_QUADS);
    glColor4fv(colour);
    glVertex3f(-1.0,-1.0, 0.0);
    glVertex3f( 1.0,-1.0, 0.0);
    glVertex3f( 1.0, 1.0, 0.0);
    glVertex3f(-1.0, 1.0, 0.0);
    glEnd();
}

/* The main drawing function. */
void DrawGLScene()
{
    const float RED [] = {1.0, 0, 0, 0.5};
    const float BLUE [] = {0.0, 0, 1, 0.5};
    const float NONE [] = {0, 0, 0, 0};

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glTranslatef(0, 0, -10.0f);		// Move Left 1.5 Units And Into The Screen 6.0

    glPushMatrix();
    glTranslatef(-4.0, 0, -10);
    DrawQuad(RED);
    glPopMatrix();

    glTranslatef(4.0, 0, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw 3 overlapping quads, 2 of which should be totally transparent so the
     * output should be the third */
    DrawQuad(NONE);
    DrawQuad(NONE);
    DrawQuad(BLUE);

    glDisable(GL_BLEND);

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        DrawGLScene();
    }

    return 0;
}
