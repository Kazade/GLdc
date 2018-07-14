#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gl.h"
#include "glu.h"
#include "glkos.h"


/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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

/* The main drawing function. */
void DrawGLScene()
{
    static GLfloat movement = 0.0f;
    static GLboolean increasing = GL_TRUE;

    if(movement > 10.0) {
        increasing = GL_FALSE;
    } else if(movement < -10.0f) {
        increasing = GL_TRUE;
    }

    if(increasing) {
        movement += 0.05f;
    } else {
        movement -= 0.05f;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glDisable(GL_CULL_FACE);

    glPushMatrix();
        glTranslatef(0.0f, -1.0f, movement);

        glBegin(GL_TRIANGLES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3f(0.0f, 0.0f, -5.0f);

            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3f(-2.5f, 0.0f, 5.0f);

            glColor3f(0.0f, 0.0f, 1.0f);
            glVertex3f(2.5f, 0.0f, 5.0f);
        glEnd();
    glPopMatrix();

    // swap buffers to display, since we're double buffered.
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
