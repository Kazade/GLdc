#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glkos.h"

#include "../loadbmp.h"

#define SCREEN_W 640
#define SCREEN_H 480

/*
 * SDL's testviewport.c waits 1000ms between viewport changes.
 * This GLdc version uses a frame counter instead so it works cleanly
 * without depending on SDL timers.
 */
#define VIEWPORT_HOLD_FRAMES 60

#ifdef _arch_dreamcast
#define SPRITE_PATH "/rd/sprite.bmp"
#else
#define SPRITE_PATH "../samples/viewport/romdisk/sprite.bmp"
#endif

static GLuint sprite_tex = 0;
static int sprite_w = 0;
static int sprite_h = 0;

static void check_gl(const char *where)
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

    /*
     * Match SDL_Renderer-style local drawing coordinates inside the viewport:
     * x grows right, y grows down.
     */
    glOrtho(0.0, (GLdouble)width, (GLdouble)height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void setup_2d_state(void)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
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

static void draw_point_rect(float x, float y, GLubyte r, GLubyte g, GLubyte b)
{
    /*
     * SDL's test draws points. Use tiny rects so the markers are visible on
     * Dreamcast/video capture.
     */
    draw_solid_rect(x - 2.0f, y - 2.0f, 4.0f, 4.0f, r, g, b);
}

static void draw_textured_rect(GLuint texture,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4ub(255, 255, 255, 255);

    glBegin(GL_TRIANGLES);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);

    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glEnd();
}

static int load_sprite_texture(void)
{
    Image image;
    unsigned y;

    if(!ImageLoad((char *)SPRITE_PATH, &image)) {
        printf("Failed to load %s\n", SPRITE_PATH);
        return 0;
    }

    /*
     * loadbmp.h returns 24-bit RGB data for the test icon. BMP files are
     * stored bottom-up, so flip rows for normal OpenGL texture coordinates.
     */
    for(y = 0; y < image.sizeY / 2; ++y) {
        unsigned opposite = image.sizeY - 1 - y;
        char *row = image.data + (y * image.sizeX * 3);
        char *other = image.data + (opposite * image.sizeX * 3);
        unsigned x;

        for(x = 0; x < image.sizeX * 3; ++x) {
            char tmp = row[x];
            row[x] = other[x];
            other[x] = tmp;
        }
    }

    glGenTextures(1, &sprite_tex);
    glBindTexture(GL_TEXTURE_2D, sprite_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    /*
     * This follows the Dreamcast GLdc sample style: source pixels are RGB888,
     * stored internally as GL_RGB565_KOS.
     */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS,
                 (GLsizei)image.sizeX, (GLsizei)image.sizeY, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, image.data);

    sprite_w = (int)image.sizeX;
    sprite_h = (int)image.sizeY;

    free(image.data);
    check_gl("load sprite texture");

    printf("Loaded sprite %dx%d from %s\n", sprite_w, sprite_h, SPRITE_PATH);

    return 1;
}

#ifdef _arch_dreamcast
static maple_device_t *controller;

static int start_pressed(void)
{
    cont_state_t *state;

    if(!controller || !controller->valid)
        controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if(!controller)
        return 0;

    state = (cont_state_t *)maple_dev_status(controller);

    return state && (state->buttons & CONT_START);
}
#endif

static void set_sdl_style_viewport(int x, int y, int w, int h)
{
    /*
     * SDL viewport x/y are top-left window coordinates.
     * OpenGL glViewport x/y are bottom-left window coordinates.
     */
    glViewport(x, SCREEN_H - y - h, w, h);
}

static void set_sdl_style_scissor(int x, int y, int w, int h)
{
    /*
     * SDL clip/scissor x/y are top-left window coordinates.
     * OpenGL glScissor x/y are bottom-left window coordinates.
     */
    glScissor(x, SCREEN_H - y - h, w, h);
}

static void render_sdl_viewport_test(int viewport_index)
{
    static const GLubyte bg_colors[4][3] = {
        { 128, 128, 128 },
        { 128,  80,  80 },
        {  80, 128,  80 },
        {  80,  80, 128 }
    };

    /*
     * Match SDL testviewport.c:
     *
     * viewport.x = j * 100;
     * viewport.y = viewport.x;
     * viewport.w = 100 + j * 50;
     * viewport.h = 100 + j * 50;
     */
    const int viewport_x = viewport_index * 100;
    const int viewport_y = viewport_x;
    const int viewport_w = 100 + viewport_index * 50;
    const int viewport_h = 100 + viewport_index * 50;

    /*
     * Match the SDL sprite/clip rect:
     *
     * SDL_QueryTexture(sprite, NULL, NULL, &rect.w, &rect.h);
     * rect.x = (viewport.w - rect.w) / 2;
     * rect.y = (viewport.h - rect.h) / 2;
     * SDL_RenderSetClipRect(renderer, &rect);
     * SDL_RenderCopy(renderer, sprite, NULL, &rect);
     *
     * In SDL, the clip rect is viewport-local. Convert it to window
     * coordinates before calling glScissor().
     */
    const int sprite_x = (viewport_w - sprite_w) / 2;
    const int sprite_y = (viewport_h - sprite_h) / 2;
    const int screen_clip_x = viewport_x + sprite_x;
    const int screen_clip_y = viewport_y + sprite_y;

    /*
     * Clear the full screen first so old viewport positions do not remain.
     * SDL's test does not necessarily do this every loop, but it makes GLdc
     * viewport/scissor behavior much easier to inspect.
     */
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    setup_ortho(SCREEN_W, SCREEN_H);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /*
     * SDL_RenderSetViewport(renderer, &viewport)
     */
    set_sdl_style_viewport(viewport_x, viewport_y, viewport_w, viewport_h);
    setup_2d_state();
    setup_ortho(viewport_w, viewport_h);

    /*
     * SDL_SetRenderDrawColor(... 0x80, 0x80, 0x80 ...)
     * SDL_RenderClear(renderer)
     *
     * Use a full viewport-local rectangle instead of glClear so the test is
     * explicit about the current projection and viewport transform.
     */
    draw_solid_rect(0.0f, 0.0f, (float)viewport_w, (float)viewport_h,
                    bg_colors[viewport_index][0],
                    bg_colors[viewport_index][1],
                    bg_colors[viewport_index][2]);

    /*
     * SDL viewport border and marker equivalents.
     */
    draw_solid_rect(1.0f, 0.0f, (float)(viewport_w - 2), 1.0f, 0, 255, 0);
    draw_solid_rect(1.0f, (float)(viewport_h - 1),
                    (float)(viewport_w - 2), 1.0f, 0, 255, 0);
    draw_solid_rect(0.0f, 1.0f, 1.0f, (float)(viewport_h - 2), 0, 255, 0);
    draw_solid_rect((float)(viewport_w - 1), 1.0f,
                    1.0f, (float)(viewport_h - 2), 0, 255, 0);

    /*
     * Blue diagonals, like the SDL test.
     */
    glDisable(GL_TEXTURE_2D);
    glColor4ub(0, 0, 255, 255);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((float)(viewport_w - 1), (float)(viewport_h - 1));
    glVertex2f((float)(viewport_w - 1), 0.0f);
    glVertex2f(0.0f, (float)(viewport_h - 1));
    glEnd();

    /*
     * Yellow top box, like the SDL test.
     */
    draw_solid_rect((float)((viewport_w - 8) / 2), 0.0f,
                    8.0f, 8.0f, 255, 255, 0);

    /*
     * Yellow "inside point" markers, made visible as tiny rects.
     */
    draw_point_rect((float)(viewport_h / 2 + 20),
                    (float)(viewport_w / 2), 255, 255, 0);
    draw_point_rect((float)(viewport_h / 2 - 20),
                    (float)(viewport_w / 2), 255, 255, 0);
    draw_point_rect((float)(viewport_h / 2),
                    (float)(viewport_w / 2 - 20), 255, 255, 0);
    draw_point_rect((float)(viewport_h / 2),
                    (float)(viewport_w / 2 + 20), 255, 255, 0);

    /*
     * SDL_RenderSetClipRect(renderer, &rect)
     *
     * The rect is viewport-local in SDL, so add viewport_x/y first, then flip
     * for OpenGL's bottom-left glScissor coordinates.
     */
    glEnable(GL_SCISSOR_TEST);
    set_sdl_style_scissor(screen_clip_x, screen_clip_y, sprite_w, sprite_h);

    /*
     * SDL_RenderCopy(renderer, sprite, NULL, &rect)
     */
    draw_textured_rect(sprite_tex,
                       (float)sprite_x, (float)sprite_y,
                       (float)sprite_w, (float)sprite_h,
                       0.0f, 0.0f, 1.0f, 1.0f);

    /*
     * SDL_RenderSetClipRect(renderer, NULL)
     */
    glDisable(GL_SCISSOR_TEST);

    check_gl("render SDL-style viewport test");
}

int main(int argc, char **argv)
{
    int viewport_index = 0;
    unsigned frame = 0;
    unsigned hold_frame = 0;

    (void)argc;
    (void)argv;

    glKosInit();
    setup_2d_state();

    if(!load_sprite_texture())
        return EXIT_FAILURE;

#ifdef _arch_dreamcast
    controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
#endif

    printf("[sample] SDL-style viewport/scissor test\n");
    printf("[sample] viewport hold=%d frames\n", VIEWPORT_HOLD_FRAMES);
    printf("INFO: Current Viewport x=0 y=0 w=100 h=100\n");

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed())
            break;
#endif

        render_sdl_viewport_test(viewport_index);
        glKosSwapBuffers();

        frame++;
        hold_frame++;

        if(hold_frame >= VIEWPORT_HOLD_FRAMES) {
            int viewport_x;
            int viewport_y;
            int viewport_w;
            int viewport_h;

            hold_frame = 0;
            viewport_index = (viewport_index + 1) % 4;

            viewport_x = viewport_index * 100;
            viewport_y = viewport_x;
            viewport_w = 100 + viewport_index * 50;
            viewport_h = 100 + viewport_index * 50;

            printf("INFO: Current Viewport x=%d y=%d w=%d h=%d frame=%u\n",
                   viewport_x, viewport_y, viewport_w, viewport_h, frame);
        }
    }

    if(sprite_tex)
        glDeleteTextures(1, &sprite_tex);

    return EXIT_SUCCESS;
}
