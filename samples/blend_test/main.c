/*
 * This sample demonstrates blending, and the importance of drawing order,
 * depth testing and z-value.
 * This is a merge of lerabot_blend_test and blend_test, with 1 added case,
 * and with adapted/corrected explanation
 */

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

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

int check_start() {
#ifdef __DREAMCAST__
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if(cont) {
        state = (cont_state_t *)maple_dev_status(cont);

        if(state)
            return state->buttons & CONT_START;
    }
#endif

    return 0;
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

    // LEFT UPPER SECTION
    glLoadIdentity();				// Reset The View
    glTranslatef(-4.0, 2.0, -10);
    // This draws 2 quads, a red first, then an overlapping blue one.
    // Both quads are drawn at the SAME z-value
    // With depth test GL_LEQUAL, this means blending for the overlapping part
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawQuad(RED);
    glTranslatef(1.0, 0, 0);
    DrawQuad(BLUE);
    glDisable(GL_BLEND);

    // RIGHT UPPER SECTION
    glTranslatef(4.0, 0, 0);
    // This draws 2 quads, a red first, then an overlapping blue one.
    // The blue quad has a LOWER z-value, so it is behind the red quad.
    // With depth test GL_LEQUAL, the blue part is not considered for the overlapping part, so no blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawQuad(RED);
    glTranslatef(1.0, 0, -0.01);
    DrawQuad(BLUE);
    glDisable(GL_BLEND);

    // LEFT DOWN SECTION
    glLoadIdentity();				// Reset The View
    glTranslatef(-4.0, -1.0, -10);
    // This draws 2 quads, a red first, then an overlapping blue one.
    // The blue quad has a HIGHER z-value, so it is in front the red quad.
    // With depth test GL_LEQUAL, this means blending for the overlapping part
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawQuad(RED);
    glTranslatef(1.0, 0, 0.01);
    DrawQuad(BLUE);
    glDisable(GL_BLEND);

    // RIGHT DOWN SECTION
    glTranslatef(4.0, 0.0, -0.01);
    // This is basically the same as the RIGHT UPPER SECTION, except that the blue quad
    //  is drawn first.
    // With depth test GL_LEQUAL, this means blending for the overlapping part 
    // <- the order of drawing is important for blending !
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTranslatef(1.0, 0.0, -0.01);
    DrawQuad(BLUE);
    glTranslatef(-1.0, 0.0, 0.01);
    DrawQuad(RED);
    glDisable(GL_BLEND);

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        if(check_start())
            break;

        DrawGLScene();
    }

    return 0;
}
