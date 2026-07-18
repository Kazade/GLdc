#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480

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

static void init_gl(void)
{
    glKosInit();

    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, 0.0, SCREEN_H, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void draw_ccw_triangle(GLfloat center_x)
{
    glColor3ub(0, 255, 0);
    glBegin(GL_TRIANGLES);
        glVertex2f(center_x - 65.0f, 265.0f);
        glVertex2f(center_x + 65.0f, 265.0f);
        glVertex2f(center_x,         405.0f);
    glEnd();
}

static void draw_cw_triangle(GLfloat center_x)
{
    glColor3ub(255, 0, 0);
    glBegin(GL_TRIANGLES);
        glVertex2f(center_x - 65.0f, 75.0f);
        glVertex2f(center_x,         215.0f);
        glVertex2f(center_x + 65.0f, 75.0f);
    glEnd();
}

static void draw_pair(GLfloat center_x)
{
    draw_ccw_triangle(center_x);
    draw_cw_triangle(center_x);
}

static void draw_separators(void)
{
    glDisable(GL_CULL_FACE);
    glColor3ub(255, 255, 255);

    glBegin(GL_LINES);
        glVertex2f(213.0f, 30.0f);
        glVertex2f(213.0f, 450.0f);
        glVertex2f(426.0f, 30.0f);
        glVertex2f(426.0f, 450.0f);
    glEnd();
}

static void draw_scene(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Left: culling disabled, so both winding orders are visible. */
    glDisable(GL_CULL_FACE);
    draw_pair(106.0f);

    /* Center: CCW is front-facing and back faces are culled. */
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    draw_pair(320.0f);

    /* Right: front faces are culled, leaving only the CW triangle. */
    glCullFace(GL_FRONT);
    draw_pair(533.0f);

    draw_separators();
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    init_gl();

    while(!check_start()) {
        draw_scene();
    }

    return 0;
}
