#include "gl.h"
#include "glu.h"
#include "glkos.h"

GLfloat rtri;
GLfloat rquad;

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LESS);				// The Type Of Depth Test To Do
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

    glTranslatef(-1.5f,0.0f,-6.0f);		// Move Left 1.5 Units And Into The Screen 6.0

    // draw a triangle (in smooth coloring mode)
    glRotatef(rtri, 0.0f,1.0f,0.0f);
    glBegin(GL_TRIANGLES);				// start drawing a polygon
        glColor3f(1.0f,0.0f,0.0f);			// Set The Color To Red
        glVertex3f( 0.0f, 1.0f, 0.0f);		// Top
        glColor3f(0.0f,1.0f,0.0f);			// Set The Color To Green
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glColor3f(0.0f,0.0f,1.0f);			// Set The Color To Blue
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd();					// we're done with the polygon (smooth color interpolation)

    glTranslatef(3.0f,0.0f,0.0f);		        // Move Right 3 Units

    glLoadIdentity();
    glTranslatef(1.5f,0.0f,-6.0f);
    glRotatef(rquad,1.0f,0.0f,0.0f);

    // draw a square (quadrilateral)
    glColor3f(0.5f,0.5f,1.0f);			// set color to a blue shade.
    glBegin(GL_QUADS);				// start drawing a polygon (4 sided)
        glVertex3f(-1.0f, 1.0f, 0.0f);		// Top Left
        glVertex3f( 1.0f, 1.0f, 0.0f);		// Top Right
        glVertex3f( 1.0f,-1.0f, 0.0f);		// Bottom Right
        glVertex3f(-1.0f,-1.0f, 0.0f);		// Bottom Left
    glEnd(); 				// done with the polygon

    rtri += 0.2f;
    rquad -= 0.15f;

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
