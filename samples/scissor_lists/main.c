#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480

#define SCISSOR_X 17
#define SCISSOR_Y 19
#define SCISSOR_W 101
#define SCISSOR_H 73

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

static void setup_ortho(void)
{
    glViewport(0, 0, SCREEN_W, SCREEN_H);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, SCREEN_H, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void init_gl(void)
{
    glKosInit();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    setup_ortho();
}

static void draw_rect(float x0, float y0, float x1, float y1,
                      GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    glColor4ub(r, g, b, a);

    glBegin(GL_QUADS);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
    glEnd();
}

static void draw_scissor_outline(void)
{
    GLfloat x = (GLfloat)SCISSOR_X;
    GLfloat y = (GLfloat)(SCREEN_H - SCISSOR_Y - SCISSOR_H);
    GLfloat w = (GLfloat)SCISSOR_W;
    GLfloat h = (GLfloat)SCISSOR_H;

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
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

static void draw_scene(void)
{
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_scissor_outline();

    glEnable(GL_SCISSOR_TEST);
    glScissor(SCISSOR_X, SCISSOR_Y, SCISSOR_W, SCISSOR_H);

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    draw_rect(0.0f, 0.0f, 50.0f, SCREEN_H, 0, 0, 255, 255);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    draw_rect(50.0f, 0.0f, 85.0f, SCREEN_H, 0, 255, 0, 255);
    glDisable(GL_ALPHA_TEST);

    glEnable(GL_BLEND);
    draw_rect(85.0f, 0.0f, SCREEN_W, SCREEN_H, 255, 0, 0, 192);

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
