#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include <stdio.h>

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

static void setup(void)
{
    glKosInit();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, SCREEN_H, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_SCISSOR_TEST);
}

static void rect_quad(float x0, float y0, float x1, float y1)
{
    glBegin(GL_QUADS);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
    glEnd();
}

static void rect_triangles(float x0, float y0, float x1, float y1)
{
    glBegin(GL_TRIANGLES);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);

        glVertex2f(x0, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
    glEnd();
}

static void rect_polygon4(float x0, float y0, float x1, float y1)
{
    glBegin(GL_POLYGON);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
    glEnd();
}

static void octagon_points(float cx, float cy, float r)
{
    glVertex2f(cx + 0.00f * r, cy - 1.00f * r);
    glVertex2f(cx + 0.71f * r, cy - 0.71f * r);
    glVertex2f(cx + 1.00f * r, cy + 0.00f * r);
    glVertex2f(cx + 0.71f * r, cy + 0.71f * r);
    glVertex2f(cx + 0.00f * r, cy + 1.00f * r);
    glVertex2f(cx - 0.71f * r, cy + 0.71f * r);
    glVertex2f(cx - 1.00f * r, cy + 0.00f * r);
    glVertex2f(cx - 0.71f * r, cy - 0.71f * r);
}

static void fan8(float cx, float cy, float r)
{
    glBegin(GL_TRIANGLE_FAN);
        octagon_points(cx, cy, r);
    glEnd();
}

static void polygon8(float cx, float cy, float r)
{
    glBegin(GL_POLYGON);
        octagon_points(cx, cy, r);
    glEnd();
}

static void draw_case(float y, void (*draw)(float, float, float, float),
                      float x0, float y0, float x1, float y1,
                      GLubyte r, GLubyte g, GLubyte b)
{
    glColor4ub(r, g, b, 255);
    draw(x0, y0 + y, x1, y1 + y);
}

static void draw_scene(void)
{
    static int announced = 0;

    if(!announced) {
        printf("case 1: GL_QUADS cyan\n");
        printf("case 2: GL_POLYGON(4) cyan\n");
        printf("case 3: GL_TRIANGLES cyan\n");
        printf("case 4: GL_POLYGON(4) blue\n");
        printf("case 5: GL_POLYGON(4) magenta\n");
        printf("case 6: GL_TRIANGLE_FAN(8) cyan\n");
        printf("case 7: GL_POLYGON(8) cyan\n");
        announced = 1;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    draw_case(0.0f,   rect_quad,      40.0f,  40.0f, 140.0f, 140.0f, 0,   255, 255);
    draw_case(0.0f,   rect_polygon4, 180.0f, 40.0f, 280.0f, 140.0f, 0,   255, 255);
    draw_case(0.0f,   rect_triangles, 320.0f, 40.0f, 420.0f, 140.0f, 0,   255, 255);
    draw_case(150.0f, rect_polygon4,  40.0f,  40.0f, 140.0f, 140.0f, 0,   0,   255);
    draw_case(150.0f, rect_polygon4, 180.0f, 40.0f, 280.0f, 140.0f, 255, 0,   255);
    glColor4ub(0, 255, 255, 255);
    fan8(360.0f, 205.0f, 42.0f);
    glColor4ub(0, 255, 255, 255);
    polygon8(520.0f, 205.0f, 42.0f);

    glKosSwapBuffers();
}

int main(void)
{
    setup();

    while(!check_start()) {
        draw_scene();
    }

    return 0;
}
