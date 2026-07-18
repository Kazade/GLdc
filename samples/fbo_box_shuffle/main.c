#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glu.h"
#include "GL/glkos.h"

#include "../loadbmp.h"

#define SCREEN_W 640
#define SCREEN_H 480

#define GRID_TILES 2
#define TILE_COUNT 4
#define TILE_SIZE 64
#define GRID_TEXTURE_SIZE (GRID_TILES * TILE_SIZE)

#ifdef _arch_dreamcast
#define LETTERS_BMP_PATH "/rd/letters.bmp"
#else
#define LETTERS_BMP_PATH "samples/fbo_box_shuffle/romdisk/letters.bmp"
#endif

static GLuint g_source_texture = 0;
static GLuint g_tile_textures[TILE_COUNT];
static GLuint g_fbo = 0;
static int g_positions[TILE_COUNT] = {0, 1, 2, 3};

static uint64_t now_ms(void)
{
#ifdef _arch_dreamcast
    return timer_ms_gettime64();
#else
    return (uint64_t)((clock() * 1000) / CLOCKS_PER_SEC);
#endif
}

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
    glDepthMask(GL_FALSE);
    glShadeModel(GL_FLAT);
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

    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glEnd();
}

static void draw_solid_rect(float x, float y, float w, float h,
                            GLubyte r, GLubyte g, GLubyte b)
{
    glDisable(GL_TEXTURE_2D);
    glColor4ub(r, g, b, 255);

    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

static int load_source_texture(void)
{
    Image image;

    if(!ImageLoad((char*)LETTERS_BMP_PATH, &image)) {
        printf("Failed to load %s\n", LETTERS_BMP_PATH);
        return 0;
    }

    if(image.sizeX != GRID_TEXTURE_SIZE || image.sizeY != GRID_TEXTURE_SIZE) {
        printf("Unexpected letters.bmp size: %ux%u\n", image.sizeX, image.sizeY);
        free(image.data);
        return 0;
    }

    /*
     * The shared sample BMP loader reads BMP rows in file order. BMP files are
     * bottom-up, while this sample treats texture coordinate v=0 as the top of
     * the atlas, so flip rows before upload.
     */
    for(unsigned int y = 0; y < image.sizeY / 2; ++y) {
        unsigned int opposite = image.sizeY - 1 - y;
        char* row = image.data + (y * image.sizeX * 3);
        char* other = image.data + (opposite * image.sizeX * 3);

        for(unsigned int x = 0; x < image.sizeX * 3; ++x) {
            char tmp = row[x];
            row[x] = other[x];
            other[x] = tmp;
        }
    }

    glGenTextures(1, &g_source_texture);
    glBindTexture(GL_TEXTURE_2D, g_source_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS,
                 (GLsizei)image.sizeX, (GLsizei)image.sizeY, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, image.data);
    check_gl("load source texture");

    free(image.data);
    return 1;
}

static void create_tile_textures(void)
{
    glGenTextures(TILE_COUNT, g_tile_textures);

    for(int i = 0; i < TILE_COUNT; ++i) {
        glBindTexture(GL_TEXTURE_2D, g_tile_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, TILE_SIZE, TILE_SIZE, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    }
}

static int capture_tile_to_texture(int tile)
{
    const int tile_x = (tile % GRID_TILES) * TILE_SIZE;
    const int tile_y = (tile / GRID_TILES) * TILE_SIZE;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, g_tile_textures[tile], 0);
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

    glViewport(0, 0, TILE_SIZE, TILE_SIZE);
    setup_2d_state();
    setup_ortho(TILE_SIZE, TILE_SIZE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /*
     * Draw the full source atlas shifted so the desired 64x64 tile appears
     * inside the FBO viewport. The fake-FBO resolve then copies only the
     * attached 64x64 texture size.
     */
    draw_textured_rect(g_source_texture,
                       (float)-tile_x,
                       (float)-tile_y,
                       (float)GRID_TEXTURE_SIZE,
                       (float)GRID_TEXTURE_SIZE,
                       0.0f, 0.0f,
                       1.0f, 1.0f);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("capture tile");
    return 1;
}

static int capture_all_tiles(void)
{
    glGenFramebuffersEXT(1, &g_fbo);

    for(int i = 0; i < TILE_COUNT; ++i) {
        if(!capture_tile_to_texture(i)) {
            return 0;
        }
    }

    return 1;
}

static void shuffle_positions(void)
{
    int a = rand() % TILE_COUNT;
    int b = rand() % TILE_COUNT;

    while(b == a) {
        b = rand() % TILE_COUNT;
    }

    int tmp = g_positions[a];
    g_positions[a] = g_positions[b];
    g_positions[b] = tmp;

    // printf("shuffle swap %d <-> %d: [%d, %d, %d, %d]\n",
    //        a, b,
    //        g_positions[0], g_positions[1],
    //        g_positions[2], g_positions[3]);
}

static void draw_board(void)
{
    const float board_size = 320.0f;
    const float draw_tile = board_size / 2.0f;
    const float origin_x = (SCREEN_W - board_size) * 0.5f;
    const float origin_y = (SCREEN_H - board_size) * 0.5f;

    glViewport(0, 0, SCREEN_W, SCREEN_H);
    setup_2d_state();
    setup_ortho(SCREEN_W, SCREEN_H);

    glClearColor(0.02f, 0.03f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_solid_rect(origin_x - 8.0f, origin_y - 8.0f,
                    board_size + 16.0f, board_size + 16.0f,
                    12, 18, 26);

    for(int cell = 0; cell < TILE_COUNT; ++cell) {
        const int tile = g_positions[cell];
        const int cell_x = cell % GRID_TILES;
        const int cell_y = cell / GRID_TILES;
        const float x = origin_x + (float)cell_x * draw_tile;
        const float y = origin_y + (float)cell_y * draw_tile;

        draw_textured_rect(g_tile_textures[tile],
                           x + 4.0f, y + 4.0f,
                           draw_tile - 8.0f, draw_tile - 8.0f,
                           0.0f, 0.0f,
                           1.0f, 1.0f);
    }
}

#ifdef _arch_dreamcast
static int start_pressed(void)
{
    maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *state = cont ? (cont_state_t *)maple_dev_status(cont) : NULL;
    return state && (state->buttons & CONT_START);
}
#endif

int main(int argc, char** argv)
{
    uint64_t last_shuffle;

    (void)argc;
    (void)argv;

    glKosInit();
    srand((unsigned)now_ms());

    if(!load_source_texture()) {
        return 1;
    }

    create_tile_textures();
    if(!capture_all_tiles()) {
        return 1;
    }

    last_shuffle = now_ms();

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) {
            break;
        }
#endif

        if(now_ms() - last_shuffle >= 100) {
            shuffle_positions();
            last_shuffle = now_ms();
        }

        draw_board();
        glKosSwapBuffers();
    }

    return 0;
}
