#include <math.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define COLUMN_COUNT 10

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

static void setup_projection(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0f, 320.0f / 480.0f, 1.0f, 20.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void init_gl(void)
{
    glKosInit();

    glClearColor(0.03f, 0.03f, 0.05f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_NEARZ_CLIPPING_KOS);
    glShadeModel(GL_SMOOTH);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);

    setup_projection();
}

static void draw_ribbon(GLfloat phase)
{
    glBegin(GL_TRIANGLE_STRIP);

    for(int i = 0; i < COLUMN_COUNT; ++i) {
        const GLfloat column = (GLfloat)i / (GLfloat)(COLUMN_COUNT - 1);
        const GLfloat screen_x = -0.38f + column * 0.76f;
        const GLfloat depth = 1.15f + 0.55f * sinf(phase + (GLfloat)i * 1.1f);
        const GLfloat x = screen_x * depth;
        const GLfloat y = 0.42f * depth;

        if(i & 1)
            glColor3ub(0, 220, 255);
        else
            glColor3ub(255, 220, 0);

        /* Top then bottom gives every strip triangle a CCW front face. */
        glVertex3f(x,  y, -depth);
        glVertex3f(x, -y, -depth);
    }

    glEnd();
}

static void draw_separator(void)
{
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, 0.0, SCREEN_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3ub(255, 255, 255);
    glBegin(GL_LINES);
        glVertex2f(319.0f, 20.0f);
        glVertex2f(319.0f, 460.0f);
    glEnd();
}

static void draw_scene(GLfloat phase)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Left: reference rendering with face culling disabled. */
    glViewport(0, 0, SCREEN_W / 2, SCREEN_H);
    setup_projection();
    glDisable(GL_CULL_FACE);
    draw_ribbon(phase);

    /* Right: the identical strip with back-face culling enabled. */
    glViewport(SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H);
    setup_projection();
    glEnable(GL_CULL_FACE);
    draw_ribbon(phase);

    draw_separator();
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    GLfloat phase = 0.0f;

    (void)argc;
    (void)argv;

    init_gl();

    while(!check_start()) {
        draw_scene(phase);
        phase += 0.0125f;

        if(phase > 6.2831853f)
            phase -= 6.2831853f;
    }

    return 0;
}
