#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define POT_W 128
#define POT_H 128
#define NPOT_W 160
#define NPOT_H 120

static GLuint uploaded_texture;
static GLuint pot_texture;
static GLuint npot_texture;
static GLuint nested_texture;
static GLuint framebuffer;

static GLushort rgb565(GLubyte r, GLubyte g, GLubyte b) {
    return (GLushort)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void setup_ortho_top_down(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* SDL's OpenGL renderer uses this projection while a render target is bound. */
static void setup_ortho_render_target(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void setup_2d_state(void) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_FLAT);
}

static void draw_solid_rect(float x, float y, float w, float h,
                            GLubyte r, GLubyte g, GLubyte b) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(r, g, b, 255);

    glBegin(GL_TRIANGLES);
    glVertex2f(x, y);
    glVertex2f(x, y + h);
    glVertex2f(x + w, y);
    glVertex2f(x, y + h);
    glVertex2f(x + w, y + h);
    glVertex2f(x + w, y);
    glEnd();
}

static void draw_textured_rect(GLuint texture,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4ub(255, 255, 255, 255);

    glBegin(GL_TRIANGLES);
    glTexCoord2f(u0, v0); glVertex2f(x, y);
    glTexCoord2f(u0, v1); glVertex2f(x, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glTexCoord2f(u0, v1); glVertex2f(x, y + h);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glEnd();
}

static void draw_orientation_pattern(int width, int height) {
    draw_solid_rect(0, 0, width, height, 24, 24, 32);
    draw_solid_rect(0, 0, width, 20, 255, 32, 32);
    draw_solid_rect(0, height - 20, width, 20, 32, 96, 255);
    draw_solid_rect(10, 28, 30, 22, 32, 255, 64);
    draw_solid_rect(width - 42, height - 52, 30, 30, 255, 224, 32);
}

static int attach_target(GLuint texture, int width, int height) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, texture, 0);
    if(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("FBO incomplete for %dx%d target\n", width, height);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        return 0;
    }

    glViewport(0, 0, width, height);
    setup_ortho_render_target(width, height);
    setup_2d_state();
    return 1;
}

static int render_pattern_target(GLuint texture, int width, int height) {
    if(!attach_target(texture, width, height)) {
        return 0;
    }

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_orientation_pattern(width, height);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return 1;
}

static int render_nested_target(void) {
    if(!attach_target(nested_texture, NPOT_W, NPOT_H)) {
        return 0;
    }

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_textured_rect(pot_texture, 0, 0, NPOT_W, NPOT_H, 0, 0, 1, 1);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return 1;
}

static int create_uploaded_texture(void) {
    GLushort* pixels = (GLushort*)malloc(POT_W * POT_H * sizeof(GLushort));
    if(!pixels) {
        return 0;
    }

    for(int y = 0; y < POT_H; ++y) {
        for(int x = 0; x < POT_W; ++x) {
            GLushort color = rgb565(24, 24, 32);
            if(y < 20) color = rgb565(255, 32, 32);
            else if(y >= POT_H - 20) color = rgb565(32, 96, 255);
            if(x >= 10 && x < 40 && y >= 28 && y < 50) color = rgb565(32, 255, 64);
            if(x >= POT_W - 42 && x < POT_W - 12 &&
               y >= POT_H - 52 && y < POT_H - 22) color = rgb565(255, 224, 32);
            pixels[y * POT_W + x] = color;
        }
    }

    glGenTextures(1, &uploaded_texture);
    glBindTexture(GL_TEXTURE_2D, uploaded_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, POT_W, POT_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
    free(pixels);
    return glGetError() == GL_NO_ERROR;
}

static void create_target_texture(GLuint* texture, int width, int height,
                                  GLenum filter) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, width, height, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
}

static void draw_panel(GLuint texture, float x, GLubyte r, GLubyte g, GLubyte b) {
    draw_solid_rect(x - 4, 76, 144, 152, r, g, b);
    draw_textured_rect(texture, x, 80, 136, 144, 0, 0, 1, 1);

    /* Cropped coordinates verify that partial source rectangles use the same
     * orientation and logical-to-physical transform as the full image. */
    draw_solid_rect(x - 4, 252, 144, 112, r, g, b);
    draw_textured_rect(texture, x, 256, 136, 104,
                       0.125f, 0.125f, 0.875f, 0.875f);
}

static void draw_frame(void) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    setup_ortho_top_down(SCREEN_W, SCREEN_H);
    setup_2d_state();

    glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_panel(uploaded_texture, 12, 255, 255, 255);
    draw_panel(pot_texture, 170, 255, 224, 32);
    draw_panel(npot_texture, 328, 255, 32, 255);
    draw_panel(nested_texture, 486, 32, 255, 255);
}

#ifdef _arch_dreamcast
static int start_pressed(void) {
    maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t* state = cont ? (cont_state_t*)maple_dev_status(cont) : NULL;
    return state && (state->buttons & CONT_START);
}
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    glKosInit();
    setup_2d_state();

    if(!create_uploaded_texture()) {
        return 1;
    }

    create_target_texture(&pot_texture, POT_W, POT_H, GL_NEAREST);
    create_target_texture(&npot_texture, NPOT_W, NPOT_H, GL_LINEAR);
    create_target_texture(&nested_texture, NPOT_W, NPOT_H, GL_LINEAR);
    glGenFramebuffersEXT(1, &framebuffer);

    if(!render_pattern_target(pot_texture, POT_W, POT_H) ||
       !render_pattern_target(npot_texture, NPOT_W, NPOT_H) ||
       !render_nested_target()) {
        return 1;
    }

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) break;
#endif
        draw_frame();
        glKosSwapBuffers();
    }

    return 0;
}
