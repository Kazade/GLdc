#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glDepthFunc(GL_ALWAYS);
    DrawSquare(100, 1, 1, 1, -5.0f);

    glTranslatef(-2.0, 1.5, 0.0f);
    glDepthFunc(GL_LEQUAL);
    DrawSquare(1.0, 1, 0, 0, -5.0f);

    glPushMatrix();
        glTranslatef(0, -1.5, 0);
        DrawSquare(1.0, 1, 0, 0, -4.9f);
    glPopMatrix();

    glTranslatef(1.1, 0, 0);
    glDepthFunc(GL_EQUAL);
    DrawSquare(1.0, 1, 0, 0, -5.0f);

    glPushMatrix();
        glTranslatef(0, -1.5, 0);
        DrawSquare(1.0, 1, 0, 0, -5.0f);
    glPopMatrix();

    glTranslatef(1.1, 0, 0);
    glDepthFunc(GL_GEQUAL);
    DrawSquare(1.0, 1, 0, 0, -5.0f);

    glPushMatrix();
        glTranslatef(0, -1.5, 0);
        DrawSquare(1.0, 1, 0, 0, -5.1f);
    glPopMatrix();

    glTranslatef(1.1, 0, 0);
    glDepthFunc(GL_LESS);
    DrawSquare(1.0, 1, 0, 0, -4.9f);

    glPushMatrix();
        glTranslatef(0, -1.5, 0);
        DrawSquare(1.0, 1, 0, 0, -4.8f);
    glPopMatrix();

    glTranslatef(1.1, 0, 0);
    glDepthFunc(GL_GREATER);
    DrawSquare(1.0, 1, 0, 0, -5.1f);

    glPushMatrix();
        glTranslatef(0, -1.5, 0);
        DrawSquare(1.0, 1, 0, 0, -5.2f);
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
