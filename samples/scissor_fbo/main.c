#include <stdio.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480

#define TARGET_W 256
#define TARGET_H 120

#define SCISSOR_X 17
#define SCISSOR_Y 19
#define SCISSOR_W 101
#define SCISSOR_H 73

#define DISPLAY_X 64
#define DISPLAY_Y 64
#define DISPLAY_SCALE 2

#define DEFAULT_X 500
#define DEFAULT_Y 20
#define DEFAULT_W 101
#define DEFAULT_H 73

static GLuint target_texture;
static GLuint framebuffer;

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

static void setup_ortho(GLsizei width, GLsizei height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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

static void draw_outline(float x, float y, float w, float h)
{
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    draw_rect(x - 2.0f, y - 2.0f, x + w + 2.0f, y,
              255, 255, 0, 255);
    draw_rect(x - 2.0f, y + h, x + w + 2.0f, y + h + 2.0f,
              255, 255, 0, 255);
    draw_rect(x - 2.0f, y, x, y + h,
              255, 255, 0, 255);
    draw_rect(x + w, y, x + w + 2.0f, y + h,
              255, 255, 0, 255);
}

static int create_framebuffer(void)
{
    glGenTextures(1, &target_texture);
    glBindTexture(GL_TEXTURE_2D, target_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS,
                 TARGET_W, TARGET_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, target_texture, 0);

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if(status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("FBO status=0x%04x\n", (unsigned)status);
        if(status == GL_FRAMEBUFFER_UNSUPPORTED_EXT)
            printf("RTT framebuffer support requires KOS > 2.3.0\n");

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        return 0;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return 1;
}

static void render_scissor_target(void)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    setup_ortho(TARGET_W, TARGET_H);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(SCISSOR_X, SCISSOR_Y, SCISSOR_W, SCISSOR_H);

    draw_rect(0.0f, 0.0f, 50.0f, TARGET_H, 0, 0, 255, 255);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    draw_rect(50.0f, 0.0f, 85.0f, TARGET_H, 0, 255, 0, 255);
    glDisable(GL_ALPHA_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_rect(85.0f, 0.0f, TARGET_W, TARGET_H, 255, 0, 0, 192);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    /*
     * Bright logical edge rows make linear-filter sampling from the unused
     * POT-height region visible as a dark or corrupted fringe.
     */
    draw_rect(0.0f, 0.0f, TARGET_W, 4.0f, 255, 255, 255, 255);
    draw_rect(0.0f, TARGET_H - 4.0f, TARGET_W, TARGET_H,
              255, 255, 255, 255);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

static void draw_target_texture(void)
{
    float x0 = DISPLAY_X;
    float y0 = DISPLAY_Y;
    float x1 = DISPLAY_X + TARGET_W * DISPLAY_SCALE;
    float y1 = DISPLAY_Y + TARGET_H * DISPLAY_SCALE;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, target_texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4ub(255, 255, 255, 255);

    /* Map the OpenGL texture top (V=1) to this top-down quad's top edge. */
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y0);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y0);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y1);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y1);
    glEnd();
}

static void draw_default_framebuffer_check(void)
{
    float y0 = SCREEN_H - DEFAULT_Y - DEFAULT_H;
    float y1 = SCREEN_H - DEFAULT_Y;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(DEFAULT_X, DEFAULT_Y, DEFAULT_W, DEFAULT_H);

    draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, 0, 220, 255, 255);

    glDisable(GL_SCISSOR_TEST);
    draw_outline(DEFAULT_X, y0, DEFAULT_W, y1 - y0);
}

static void render_main_scene(void)
{
    float outline_x = DISPLAY_X + SCISSOR_X * DISPLAY_SCALE;
    float outline_y = DISPLAY_Y
                    + (TARGET_H - SCISSOR_Y - SCISSOR_H) * DISPLAY_SCALE;
    float outline_w = SCISSOR_W * DISPLAY_SCALE;
    float outline_h = SCISSOR_H * DISPLAY_SCALE;

    setup_ortho(SCREEN_W, SCREEN_H);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_target_texture();
    draw_outline(outline_x, outline_y, outline_w, outline_h);
    draw_default_framebuffer_check();

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    glKosInit();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);

    if(!create_framebuffer())
        return 1;

    while(!check_start()) {
        render_scissor_target();
        render_main_scene();
    }

    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target_texture);
    return 0;
}
