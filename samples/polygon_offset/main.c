#include <kos.h>
typedef enum
{
    false,
    true
} bool;
#include "gl.h"
#include "glu.h"
#include "glkos.h"

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height) // We call this right after our OpenGL window is created.
{
    glClearColor(0.93f, 0.93f, 0.93f, 0.0f);
    glClearDepth(1.0);       // Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);  // The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST); // Enables Depth Testing
    glShadeModel(GL_SMOOTH); // Enables Smooth Color Shading

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); // Reset The Projection Matrix

    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 1.0f, 100.0f); // Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0) // Prevent A Divide By Zero If The Window Is Too Small
        Height = 1;

    glViewport(0, 0, Width, Height); // Reset The Current Viewport And Perspective Transformation

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 1.0f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

float xrot = 0.0f;
float yrot = 0.0f;

float xdiff = 0;
float ydiff = 0;

bool offset = true;

void drawBox()
{
    glBegin(GL_QUADS);
    // FRONT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    // BACK
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    // LEFT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    // RIGHT
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    // TOP
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    // BOTTOM
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glEnd();
}

void drawPolygon()
{
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.0f);
    glEnd();
}
int frames = 0;
void check_input()
{
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if (cont)
    {
        state = (cont_state_t *)maple_dev_status(cont);

        if (state)
        {
            if (frames <= 0)
            {
                if (state->buttons & CONT_START)
                {
                    offset = !offset;
                    frames = 60;
                }
            }
        }
    }
}

/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear The Screen And The Depth Buffer
    glLoadIdentity();                                   // Reset The View

    gluLookAt(
        0.0f, 0.0f, 3.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f);

    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glColor3f(1.0f, 0.0f, 0.0f);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    drawBox();

    if (offset)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
    }

    glColor3f(0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(-0.25f, -0.25f, 0.5f);
    glScalef(0.5f, 0.5f, 0.5f);
    drawPolygon();
    glPopMatrix();

    glColor3f(1.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0.25f, 0.25f, 0.5f);
    glScalef(0.5f, 0.5f, 0.5f);
    drawPolygon();
    glPopMatrix();

    if (offset)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.5f);
    glScalef(0.5f, 0.5f, 0.5f);
    drawPolygon();
    glPopMatrix();

    if (offset)
        glDisable(GL_POLYGON_OFFSET_FILL);
#if 0
    if (offset)
    {
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
    }


    glColor3f(0.0f, 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    drawBox();

    if (offset)
        glDisable(GL_POLYGON_OFFSET_LINE);

    glFlush();
#endif
    // swap buffers to display, since we're double buffered.
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while (1)
    {
        check_input();
        DrawGLScene();
        xrot += 0.1f;
        yrot += 0.25f;
        frames--;
    }

    return 0;
}
