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


/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glTranslatef(-3.0f, 1.5f, -10.0f);		// Move Left 1.5 Units And Into The Screen 6.0

    // draw a triangle
    glBegin(GL_TRIANGLES);				// start drawing a polygon
        glVertex3f( 0.0f, 1.0f, 0.0f);		// Top
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd();					// we're done with the polygon

    glTranslatef(3.0f, 0.0f, 0.0f);		        // Move Right 3 Units

    // draw a square (quadrilateral)
    glBegin(GL_QUADS);				// start drawing a polygon (4 sided)
        glVertex3f(-1.0f, 1.0f, 0.0f);		// Top Left
        glVertex3f( 1.0f, 1.0f, 0.0f);		// Top Right
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd();					// done with the polygon

    glTranslatef(3.0f, 0.0f, 0.0f);

    glBegin(GL_POLYGON);				// start drawing a polygon (4 sided)
        glVertex3f(-0.0f, 1.0f, 0.0f);		// Top Left
        glVertex3f(-0.75f, 0.75f, 0.0f);
        glVertex3f(-1.0f, 0.0f, 0.0f);		// Top Right
        glVertex3f(-0.75f,-0.75f, 0.0f);		// Bottom Right
        glVertex3f(-0.0f,-1.0f, 0.0f);		// Bottom Left
        glVertex3f( 0.75f,-0.75f, 0.0f);		// Bottom Right
        glVertex3f( 1.0f, 0.0f, 0.0f);		// Top Right
        glVertex3f( 0.75f, 0.75f, 0.0f);
    glEnd();					// done with the polygon

    glTranslatef(-6.0f, -3.0f, 0.0f);

    // draw a triangle
    glBegin(GL_POLYGON);				// start drawing a polygon
        glVertex3f( 0.0f, 1.0f, 0.0f);		// Top
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd();					// we're done with the polygon

    glTranslatef(3.0f, 0.0f, 0.0f);		        // Move Right 3 Units

    // draw a square (quadrilateral)
    glBegin(GL_POLYGON);				// start drawing a polygon (4 sided)
        glVertex3f(-1.0f, 1.0f, 0.0f);		// Top Left
        glVertex3f( 1.0f, 1.0f, 0.0f);		// Top Right
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd();					// done with the polygon

    glTranslatef(3.0f, 0.0f, 0.0f);

    glBegin(GL_POLYGON);				// start drawing a polygon (4 sided)
        glVertex3f(-0.0f, 1.0f, 0.0f);		// Top Left
        glVertex3f(-0.75f, 0.75f, 0.0f);
        glVertex3f(-1.0f, 0.0f, 0.0f);		// Top Right
        glVertex3f(-0.75f,-0.75f, 0.0f);		// Bottom Right
        glVertex3f(-0.0f,-1.0f, 0.0f);		// Bottom Left
        glVertex3f( 0.75f,-0.75f, 0.0f);		// Bottom Right
        glVertex3f( 1.0f, 0.0f, 0.0f);		// Top Right
        glVertex3f( 0.75f, 0.75f, 0.0f);
    glEnd();					// done with the polygon
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
