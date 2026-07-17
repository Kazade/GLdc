#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480

#define SCISSOR_X 160
#define SCISSOR_Y 128
#define SCISSOR_W 320
#define SCISSOR_H 224

static void setup_projection(int width, int height)
{
    if(height == 0)
        height = 1;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void init_gl(int width, int height)
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    setup_projection(width, height);

    glEnable(GL_SCISSOR_TEST);
    glScissor(SCISSOR_X, SCISSOR_Y, SCISSOR_W, SCISSOR_H);
}

static int check_start(void)
{
#ifdef _arch_dreamcast
    static maple_device_t *cont;
    cont_state_t *state;

    if(!cont || !cont->valid)
        cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if(cont) {
        state = (cont_state_t *)maple_dev_status(cont);

        if(state)
            return state->buttons & CONT_START;
    }
#endif

    return 0;
}

static void draw_scissor_outline(void)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, SCREEN_H, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    {
        GLfloat x = (GLfloat)SCISSOR_X;
        GLfloat y = (GLfloat)(SCREEN_H - SCISSOR_Y - SCISSOR_H);
        GLfloat w = (GLfloat)SCISSOR_W;
        GLfloat h = (GLfloat)SCISSOR_H;

        glDisable(GL_TEXTURE_2D);
        glColor4ub(255, 255, 0, 255);

        glBegin(GL_LINES);
            glVertex2f(x,     y);
            glVertex2f(x + w, y);

            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);

            glVertex2f(x + w, y + h);
            glVertex2f(x,     y + h);

            glVertex2f(x, y + h);
            glVertex2f(x, y);
        glEnd();
    }

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
}

static void draw_scene_geometry(void)
{
    glLoadIdentity();

    glTranslatef(-3.0f, 1.5f, -10.0f);

    glColor4ub(255, 0, 0, 255);
    glBegin(GL_TRIANGLES);
        glVertex3f( 0.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glColor4ub(0, 255, 0, 255);
    glBegin(GL_QUADS);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glColor4ub(0, 0, 255, 255);
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

    glColor4ub(255, 128, 0, 255);
    glBegin(GL_TRIANGLES);
        glVertex3f( 0.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glColor4ub(0, 255, 255, 255);
    glBegin(GL_POLYGON);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f, 1.0f, 0.0f);
        glVertex3f( 1.0f,-1.0f, 0.0f);
        glVertex3f(-1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f, 0.0f, 0.0f);

    glColor4ub(255, 0, 255, 255);
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
}

static void draw_gl_scene(void)
{
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_scissor_outline();

    glEnable(GL_SCISSOR_TEST);
    glScissor(SCISSOR_X, SCISSOR_Y, SCISSOR_W, SCISSOR_H);

    draw_scene_geometry();

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    glKosInit();
    init_gl(SCREEN_W, SCREEN_H);

    while(1) {
        if(check_start())
            break;

        draw_gl_scene();
    }

    return 0;
}
