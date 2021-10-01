#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"


void InitGL(int Width, int Height)
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);

    glMatrixMode(GL_MODELVIEW);

    /* We cut out a square in the middle of the viewport to render to */
    glEnable(GL_SCISSOR_TEST);
    glScissor(160, 120, 320, 240);
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0)
        Height = 1;

    glViewport(0, 0, Width, Height);

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

/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(-3.0f, 1.5f, -10.0f);

    glBegin(GL_TRIANGLES);
        glVertex3f( 0.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);


    glBegin(GL_QUADS);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glBegin(GL_POLYGON);
        glVertex3f(-0.0f, 1.0f, 0.0f);
        glVertex3f(-0.75f, 0.75f, 0.0f);
        glVertex3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-0.75f,-0.75f, 0.0f);
        glVertex3f(-0.0f,-1.0f, 0.0f);
        glVertex3f( 0.75f,-0.75f, 0.0f);
        glVertex3f( 1.0f, 0.0f, 0.0f);
        glVertex3f( 0.75f, 0.75f, 0.0f);
    glEnd();

    glTranslatef(-6.0f, -3.0f, 0.0f);


    glBegin(GL_POLYGON);
        glVertex3f( 0.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);


    glBegin(GL_POLYGON);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glBegin(GL_POLYGON);
        glVertex3f(-0.0f, 1.0f, 0.0f);
        glVertex3f(-0.75f, 0.75f, 0.0f);
        glVertex3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-0.75f,-0.75f, 0.0f);
        glVertex3f(-0.0f,-1.0f, 0.0f);
        glVertex3f( 0.75f,-0.75f, 0.0f);
        glVertex3f( 1.0f, 0.0f, 0.0f);
        glVertex3f( 0.75f, 0.75f, 0.0f);
    glEnd();

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
