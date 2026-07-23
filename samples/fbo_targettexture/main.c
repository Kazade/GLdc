#include <stdint.h>
#include <stdio.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define TEX_W 64
#define TEX_H 64
#define WIN_W  640
#define WIN_H  480

#define DRAW_W 320
#define DRAW_H 240

static GLuint g_target_texture = 0;
static float g_player_x = 10.0f;
static float g_player_dx = 1.75f;

static void check_gl(const char* where)
{
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        printf("[GL] %s: error 0x%04x\n", where, (unsigned)err);
    }
}

static void setup_ortho(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)width, (GLdouble)height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void setup_2d_state(void)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_FLAT);
}

static void draw_solid_rect(float x, float y, float w, float h,
                            GLubyte r, GLubyte g, GLubyte b)
{
    glDisable(GL_TEXTURE_2D);
    glColor4ub(r, g, b, 255);

    glBegin(GL_TRIANGLES);
    glVertex2f(x,     y);
    glVertex2f(x,     y + h);
    glVertex2f(x + w, y);

    glVertex2f(x,     y + h);
    glVertex2f(x + w, y + h);
    glVertex2f(x + w, y);
    glEnd();
}

static void draw_textured_rect(GLuint texture,
                               float x,
                               float y,
                               float w,
                               float h,
                               float u0,
                               float v0,
                               float u1,
                               float v1)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4ub(255, 255, 255, 255);

    /*
     * Submit the quad as two explicit triangles so the sample avoids both the
     * GL_QUADS expansion path and strip-specific corner cases while we debug
     * RTT sampling on Dreamcast hardware.
     */
    glBegin(GL_TRIANGLES);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);

    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glEnd();
}

static int render_target_texture(GLuint framebuffer)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, g_target_texture, 0);
    {
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if(status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            printf("FBO status=0x%04x\n", (unsigned)status);
            if(status == GL_FRAMEBUFFER_UNSUPPORTED_EXT) {
                printf("RTT framebuffer support requires KOS > 2.3.0\n");
            }
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            return 0;
        }
    }

    /*
     * GLdc's fake-FBO resolve copies from:
     *
     *   srcY = framebufferHeight - viewportY - textureHeight
     *
     * Using the upper framebuffer band here keeps the temporary capture scene
     * away from the visible lower-left corner while still resolving the
     * attached 64x64 texture correctly.
     */
    glViewport(0, 0, TEX_W, TEX_H);
    setup_2d_state();
    setup_ortho(TEX_W, TEX_H);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_solid_rect(0.0f, 0.0f, TEX_W, TEX_H, 8, 12, 24);
    draw_solid_rect(g_player_x, 16.0f, 20.0f, 20.0f, 255, 0, 0);
    draw_solid_rect(g_player_x + 5.0f, 22.0f, 10.0f, 8.0f, 0, 255, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("render target texture");
    return 1;
}

static void render_default_frame(int x, int y)
{
    (void)x;
    (void)y;

    glViewport(0, 0, WIN_W, WIN_H);
    setup_2d_state();
    setup_ortho(WIN_W, WIN_H);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_textured_rect(g_target_texture,
                       100.0f, 80.0f,
                       DRAW_W, DRAW_H,
                       0.0f, 1.0f,
                       1.0f, 0.0f);

    draw_solid_rect(460.0f, 80.0f, 40.0f, 260.0f, 0, 255, 0);
}

#ifdef _arch_dreamcast
static int start_pressed(void)
{
    maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *state = cont ? (cont_state_t *)maple_dev_status(cont) : NULL;
    return state && (state->buttons & CONT_START);
}
#endif

int main(int argc, char **argv)
{
    GLuint framebuffer = 0;
    int frame = 0;
    int x = 20;
    int y = 20;
    int vx = 2;
    int vy = 1;
    int captured = 0;

    (void)argc;
    (void)argv;

    glKosInit();
    setup_2d_state();

    glGenTextures(1, &g_target_texture);
    glBindTexture(GL_TEXTURE_2D, g_target_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, TEX_W, TEX_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    check_gl("create target texture");

    glGenFramebuffersEXT(1, &framebuffer);
    captured = 1;

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) {
            break;
        }
#endif

        x += vx;
        y += vy;

        if(x < 0 || x > WIN_W - DRAW_W) {
            vx = -vx;
            x += vx;
        }

        if(y < 0 || y > WIN_H - DRAW_H) {
            vy = -vy;
            y += vy;
        }

        g_player_x += g_player_dx;
        if(g_player_x < 0.0f || g_player_x > TEX_W - 20.0f) {
            g_player_dx = -g_player_dx;
            g_player_x += g_player_dx;
        }

        if(!render_target_texture(framebuffer)) {
            return 1;
        }
        render_default_frame(x, y);
        glKosSwapBuffers();

        if((frame % 120) == 0) {
            printf("[sample] frame %d captured=%d draw=%d,%d\n",
                   frame, captured, x, y);
        }

        frame++;
    }

    return 0;
}
