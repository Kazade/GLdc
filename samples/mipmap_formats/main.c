#include <stdint.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define TEX_SIZE 64

static GLuint textures[3];

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

static void set_texture_parameters(void)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

static void create_rgb565_texture(void)
{
    GLubyte pixels[TEX_SIZE * TEX_SIZE * 3];

    for(int y = 0; y < TEX_SIZE; ++y) {
        for(int x = 0; x < TEX_SIZE; ++x) {
            const int p = (y * TEX_SIZE + x) * 3;
            const int checker = ((x >> 3) ^ (y >> 3)) & 1;
            pixels[p + 0] = checker ? 255 : 20;
            pixels[p + 1] = checker ? 180 : 80;
            pixels[p + 2] = checker ? 20 : 255;
        }
    }

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    set_texture_parameters();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_TWID_KOS,
                 TEX_SIZE, TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
}

static void create_argb4444_texture(void)
{
    GLubyte pixels[TEX_SIZE * TEX_SIZE * 4];

    for(int y = 0; y < TEX_SIZE; ++y) {
        for(int x = 0; x < TEX_SIZE; ++x) {
            const int p = (y * TEX_SIZE + x) * 4;
            const int fence = (x & 15) < 4 || (y & 15) < 4;
            pixels[p + 0] = 30;
            pixels[p + 1] = 255;
            pixels[p + 2] = 70;
            pixels[p + 3] = fence ? 255 : 0;
        }
    }

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    set_texture_parameters();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB4444_TWID_KOS,
                 TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
}

static void create_argb1555_texture(void)
{
    GLubyte pixels[TEX_SIZE * TEX_SIZE * 4];

    for(int y = 0; y < TEX_SIZE; ++y) {
        for(int x = 0; x < TEX_SIZE; ++x) {
            const int p = (y * TEX_SIZE + x) * 4;
            const int fence = (x & 15) < 4 || (y & 15) < 4;
            pixels[p + 0] = 20;
            pixels[p + 1] = 230;
            pixels[p + 2] = 255;
            pixels[p + 3] = fence ? 255 : 0;
        }
    }

    glBindTexture(GL_TEXTURE_2D, textures[2]);
    set_texture_parameters();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB1555_TWID_KOS,
                 TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
}

static void draw_solid_rect(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
                            GLubyte r, GLubyte g, GLubyte b)
{
    glDisable(GL_TEXTURE_2D);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x0, y0);
        glVertex2f(x1, y0);
        glVertex2f(x1, y1);
        glVertex2f(x0, y1);
    glEnd();
}

static void draw_background(void)
{
    const int cell = 32;
    for(int y = 0; y < SCREEN_H; y += cell) {
        for(int x = 0; x < SCREEN_W; x += cell) {
            const GLubyte c = (((x / cell) ^ (y / cell)) & 1) ? 38 : 18;
            draw_solid_rect((GLfloat)x, (GLfloat)y,
                            (GLfloat)(x + cell), (GLfloat)(y + cell),
                            c, c, (GLubyte)(c + 16));
        }
    }
}

static void draw_textured_rect(GLfloat cx, GLfloat cy, GLfloat size)
{
    const GLfloat half = size * 0.5f;

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(cx - half, cy - half);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(cx + half, cy - half);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(cx + half, cy + half);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(cx - half, cy + half);
    glEnd();
}

static void draw_column(int column)
{
    static const GLfloat centers[3] = {106.0f, 320.0f, 534.0f};
    static const GLfloat sizes[4] = {150.0f, 64.0f, 32.0f, 16.0f};
    static const GLfloat ys[4] = {365.0f, 225.0f, 145.0f, 90.0f};
    static const GLubyte marker[3][3] = {
        {255, 170, 20},
        {30, 255, 70},
        {20, 230, 255},
    };

    draw_solid_rect(centers[column] - 75.0f, 28.0f,
                    centers[column] + 75.0f, 42.0f,
                    marker[column][0], marker[column][1], marker[column][2]);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textures[column]);

    if(column == 0) {
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
    } else {
        glDisable(GL_BLEND);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5f);
    }

    /* The largest image deliberately disables mipmapping. This samples level
     * zero through baseDataOffset; the remaining images sample the mip chain
     * from its allocation base. Keeping those paths adjacent makes a bad base
     * relocation distinguishable from a bad generated level or LOD choice. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    draw_textured_rect(centers[column], ys[0], sizes[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    for(int i = 1; i < 4; ++i)
        draw_textured_rect(centers[column], ys[i], sizes[i]);
}

static void draw_scene(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_background();

    for(int column = 0; column < 3; ++column)
        draw_column(column);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    glKosInit();
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, SCREEN_W, 0.0, SCREEN_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glGenTextures(3, textures);
    create_rgb565_texture();
    create_argb4444_texture();
    create_argb1555_texture();

    while(!check_start())
        draw_scene();

    return 0;
}
