#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define TEX_SIZE 16

static GLuint texture;

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

static void create_texture(void)
{
    GLubyte pixels[TEX_SIZE * TEX_SIZE * 3];

    for(int y = 0; y < TEX_SIZE; ++y) {
        for(int x = 0; x < TEX_SIZE; ++x) {
            const int p = (y * TEX_SIZE + x) * 3;
            const int border = x == 0 || y == 0 ||
                               x == TEX_SIZE - 1 || y == TEX_SIZE - 1;
            const int diagonal = x == y || x == (TEX_SIZE - 1 - y);

            if(border || diagonal) {
                pixels[p + 0] = 255;
                pixels[p + 1] = 255;
                pixels[p + 2] = 255;
            } else if(x < TEX_SIZE / 2) {
                pixels[p + 0] = 255;
                pixels[p + 1] = 192;
                pixels[p + 2] = 0;
            } else {
                pixels[p + 0] = 0;
                pixels[p + 1] = 160;
                pixels[p + 2] = 255;
            }
        }
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_SIZE, TEX_SIZE, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

static void init_gl(void)
{
    glKosInit();

    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);

    create_texture();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, 0.0, SCREEN_H, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void draw_ccw_triangle(GLfloat center_x)
{
    glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(center_x - 65.0f, 265.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(center_x + 65.0f, 265.0f);
        glTexCoord2f(0.5f, 0.0f);
        glVertex2f(center_x, 405.0f);
    glEnd();
}

static void draw_cw_triangle(GLfloat center_x)
{
    glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(center_x - 65.0f, 75.0f);
        glTexCoord2f(0.5f, 0.0f);
        glVertex2f(center_x, 215.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(center_x + 65.0f, 75.0f);
    glEnd();
}

static void draw_pair(GLfloat center_x)
{
    draw_ccw_triangle(center_x);
    draw_cw_triangle(center_x);
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

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    init_gl();

    while(!check_start())
        draw_scene();

    return 0;
}
