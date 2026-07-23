#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _arch_dreamcast
#include <kos.h>
//~/code/dreamcast/GLdc/samples/viewport/romdisk$ /opt/toolchains/dc/kos/utils/pvrtex/pvrtex -i sprite.bmp -f rgb565 -c -o sprite.dt
#define SPRITE_PATH "/rd/sprite_argb1555.dt"
#else
#define SPRITE_PATH "../samples/fbo_simplesmile/romdisk/sprite_argb1555.dt"
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define TEX_W 256
#define TEX_H 256
#define WIN_W  640
#define WIN_H  480

#define SPRITE_W 32
#define SPRITE_H 32

#define PORTAL_X 192
#define PORTAL_Y 112
#define PORTAL_W 256
#define PORTAL_H 256

static GLuint g_portal_texture = 0;
static GLuint g_framebuffer = 0;
static GLuint g_sprite_texture = 0;

typedef struct {
    float x, y;
    float vx, vy;
    float size;
    float r, g, b;
    float phase;
} SpriteObj;

static SpriteObj g_sprites[10];
static float g_time = 0.0f;

typedef struct __attribute__((packed)) DcTxHeader {
    char fourcc[4];
    uint32_t chunk_size;
    uint8_t version;
    uint8_t header_size;
    uint8_t codebook_size;
    uint8_t colors_used;
    uint16_t width_pixels;
    uint16_t height_pixels;
    uint32_t pvr_type;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
} DcTxHeader;

typedef struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    void *buffer;
    void *data;
    GLenum format;
    GLenum internal_format;
    GLenum type;
    GLboolean compressed;
    unsigned int dataSize;
    uint32_t pvr_type;
} Image;

static void check_gl(const char* where) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        printf("[GL] %s: error 0x%04x\n", where, (unsigned)err);
    }
}

static int image_load_dt(const char *filename, Image *image) {
    FILE *file;
    long file_size;
    DcTxHeader *header;
    size_t header_size;

    file = fopen(filename, "rb");
    if(!file) {
        printf("File Not Found : %s\n", filename);
        return 0;
    }

    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    file_size = ftell(file);
    if(file_size <= 0) {
        fclose(file);
        return 0;
    }

    rewind(file);

    image->buffer = malloc((size_t)file_size);
    if(!image->buffer) {
        fclose(file);
        return 0;
    }

    if(fread(image->buffer, (size_t)file_size, 1, file) != 1) {
        fclose(file);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    fclose(file);

    if((size_t)file_size < sizeof(DcTxHeader)) {
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    header = (DcTxHeader *)image->buffer;
    if(memcmp(header->fourcc, "DcTx", 4) != 0) {
        printf("Invalid .dt file header: %s\n", filename);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    header_size = (size_t)(header->header_size + 1) * 32;
    if(header_size > (size_t)file_size || header->chunk_size > (uint32_t)file_size) {
        printf("Invalid .dt file size: %s\n", filename);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    image->sizeX = header->width_pixels;
    image->sizeY = header->height_pixels;
    image->pvr_type = header->pvr_type;
    image->data = (uint8_t *)image->buffer + header_size;
    image->dataSize = (unsigned int)(header->chunk_size - header_size);
    image->compressed = (header->pvr_type & (1u << 30)) != 0;

    {
        GLboolean twiddled = ((header->pvr_type & (1u << 26)) == 0);
        GLboolean mipmapped = ((int32_t)header->pvr_type) < 0;
        GLuint format = (header->pvr_type >> 27) & 0x7;

        if(image->compressed) {
            switch(format) {
                case 0:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS : GL_COMPRESSED_ARGB_1555_VQ_KOS);
                    break;
                case 1:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_RGB_565_VQ_TWID_KOS : GL_COMPRESSED_RGB_565_VQ_KOS);
                    break;
                case 2:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS : GL_COMPRESSED_ARGB_4444_VQ_KOS);
                    break;
                default:
                    printf("Unsupported compressed texture format in .dt: %u\n", format);
                    free(image->buffer);
                    image->buffer = NULL;
                    return 0;
            }
        } else {
            switch(format) {
                case 0:
                    image->format = GL_BGRA;
                    image->internal_format = GL_RGBA;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS : GL_UNSIGNED_SHORT_1_5_5_5_REV;
                    break;
                case 1:
                    image->format = GL_RGB;
                    image->internal_format = GL_RGB;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_5_6_5_TWID_KOS : GL_UNSIGNED_SHORT_5_6_5;
                    break;
                case 2:
                    image->format = GL_BGRA;
                    image->internal_format = GL_RGBA;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS : GL_UNSIGNED_SHORT_4_4_4_4_REV;
                    break;
                default:
                    printf("Unsupported uncompressed texture format in .dt: %u\n", format);
                    free(image->buffer);
                    image->buffer = NULL;
                    return 0;
            }
        }
    }

    return 1;
}

static void load_sprite_texture(void) {
    Image image;
    memset(&image, 0, sizeof(image));

    if(!image_load_dt(SPRITE_PATH, &image)) {
        exit(1);
    }

    printf("Loaded sprite.dt: %lux%lu compressed=%u pvr_type=0x%08lx internal=0x%04x data=%u\n",
           image.sizeX, image.sizeY, (unsigned)image.compressed,
           (unsigned long)image.pvr_type, (unsigned)image.internal_format,
           image.dataSize);

    glGenTextures(1, &g_sprite_texture);
    glBindTexture(GL_TEXTURE_2D, g_sprite_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    if(image.compressed) {
        glCompressedTexImage2DARB(GL_TEXTURE_2D, 0, image.internal_format,
                                  image.sizeX, image.sizeY, 0,
                                  image.dataSize, image.data);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, image.internal_format,
                     image.sizeX, image.sizeY, 0,
                     image.format, image.type, image.data);
    }

    free(image.buffer);
    check_gl("load sprite texture");
}

static void setup_ortho(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)width, (GLdouble)height, 0.0, -100.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void setup_2d_state(void) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_FLAT);
}

static void draw_solid_rect(float x, float y, float w, float h,
                            GLubyte r, GLubyte g, GLubyte b) {
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

static void draw_solid_rect_alpha(float x, float y, float w, float h,
                                  GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(r, g, b, a);

    glBegin(GL_TRIANGLES);
    glVertex2f(x,     y);
    glVertex2f(x,     y + h);
    glVertex2f(x + w, y);

    glVertex2f(x,     y + h);
    glVertex2f(x + w, y + h);
    glVertex2f(x + w, y);
    glEnd();
}

static void draw_textured_rect_env(GLuint texture,
                                   float x, float y, float w, float h,
                                   float u0, float v0, float u1, float v1,
                                   GLubyte r, GLubyte g, GLubyte b,
                                   GLenum env_mode) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env_mode);
    glColor4ub(r, g, b, 255);

    glBegin(GL_TRIANGLES);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);

    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glEnd();
}

static void draw_sprite_replaced(float x, float y, float size) {
    draw_textured_rect_env(g_sprite_texture,
                           x - size * 0.5f, y - size * 0.5f,
                           size, size,
                           0.0f, 0.0f, 1.0f, 1.0f,
                           255, 255, 255, GL_REPLACE);
}

static void draw_sprite_modulated(float x, float y, float size,
                                  GLubyte r, GLubyte g, GLubyte b) {
    draw_textured_rect_env(g_sprite_texture,
                           x - size * 0.5f, y - size * 0.5f,
                           size, size,
                           0.0f, 0.0f, 1.0f, 1.0f,
                           r, g, b, GL_MODULATE);
}

static void draw_2d_scene_to_texture(void) {
    draw_solid_rect(0, 0, TEX_W, TEX_H, 8, 12, 24);
    draw_solid_rect(0, 0, TEX_W, 18, 160, 30, 30);
    draw_solid_rect(0, TEX_H - 18, TEX_W, 18, 160, 30, 30);

    draw_solid_rect(8, 8, TEX_W - 16, 2, 42, 54, 92);
    draw_solid_rect(8, TEX_H - 10, TEX_W - 16, 2, 42, 54, 92);
    draw_solid_rect(8, 8, 2, TEX_H - 16, 42, 54, 92);
    draw_solid_rect(TEX_W - 10, 8, 2, TEX_H - 16, 42, 54, 92);

    draw_solid_rect(TEX_W * 0.5f - 56.0f, TEX_H * 0.5f - 6.0f, 112.0f, 12.0f, 255, 220, 0);
    draw_solid_rect(TEX_W * 0.5f - 6.0f, TEX_H * 0.5f - 56.0f, 12.0f, 112.0f, 255, 220, 0);
}

static void render_portal_texture(void) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_framebuffer);

    glViewport(0, 0, TEX_W, TEX_H);
    setup_2d_state();
    setup_ortho(TEX_W, TEX_H);

    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_2d_scene_to_texture();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for(int i = 0; i < 10; i++) {
        SpriteObj *sprite = &g_sprites[i];
        float pulse = 1.0f + 0.12f * sinf(g_time * 3.0f + sprite->phase);

        draw_sprite_modulated(sprite->x,
                              sprite->y,
                              sprite->size * pulse,
                              (GLubyte)(sprite->r * 255.0f),
                              (GLubyte)(sprite->g * 255.0f),
                              (GLubyte)(sprite->b * 255.0f));
    }
    glDisable(GL_BLEND);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    
    /* Restore viewport for main scene rendering */
    glViewport(0, 0, WIN_W, WIN_H);
    check_gl("render portal texture");
}

static void render_main_scene(void) {
    glViewport(0, 0, WIN_W, WIN_H);
    setup_2d_state();
    setup_ortho(WIN_W, WIN_H);

    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_solid_rect(0, 0, WIN_W, WIN_H, 10, 10, 20);
    draw_solid_rect(0, 0, WIN_W, 20, 40, 30, 60);
    draw_solid_rect(0, WIN_H - 20, WIN_W, 20, 40, 30, 60);
    draw_solid_rect(0, 0, 20, WIN_H, 40, 30, 60);
    draw_solid_rect(WIN_W - 20, 0, 20, WIN_H, 40, 30, 60);
    draw_solid_rect(214, 10, 212, 20, 100, 80, 200);

    {
        float border = 8.0f;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        draw_solid_rect_alpha(PORTAL_X - border * 2, PORTAL_Y - border * 2,
                              PORTAL_W + border * 4, PORTAL_H + border * 4,
                              30, 10, 60, 100);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        draw_solid_rect(PORTAL_X - border, PORTAL_Y - border,
                        PORTAL_W + border * 2, border, 80, 50, 150);
        draw_solid_rect(PORTAL_X - border, PORTAL_Y + PORTAL_H,
                        PORTAL_W + border * 2, border, 80, 50, 150);
        draw_solid_rect(PORTAL_X - border, PORTAL_Y,
                        border, PORTAL_H, 80, 50, 150);
        draw_solid_rect(PORTAL_X + PORTAL_W, PORTAL_Y,
                        border, PORTAL_H, 80, 50, 150);
    }

    glDisable(GL_BLEND);
    draw_textured_rect_env(g_portal_texture,
                           PORTAL_X, PORTAL_Y,
                           PORTAL_W, PORTAL_H,
                           0.0f, 1.0f, 1.0f, 0.0f,
                           255, 255, 255, GL_MODULATE);

    draw_sprite_replaced(72.0f, 92.0f, 64.0f);

    glDisable(GL_BLEND);
}

static void init_sprites(void) {
    float colors[10][3] = {
        {1.0f, 0.2f, 0.2f},
        {0.2f, 1.0f, 0.2f},
        {0.2f, 0.45f, 1.0f},
        {1.0f, 1.0f, 0.2f},
        {1.0f, 0.2f, 1.0f},
        {0.2f, 1.0f, 1.0f},
        {1.0f, 0.55f, 0.2f},
        {0.55f, 0.2f, 1.0f},
        {0.9f, 0.9f, 0.9f},
        {0.5f, 1.0f, 0.4f}
    };

    for(int i = 0; i < 10; i++) {
        g_sprites[i].x = 34.0f + (float)(i % 5) * 47.0f;
        g_sprites[i].y = 48.0f + (float)(i / 5) * 96.0f;
        g_sprites[i].vx = (18.0f + i * 2.5f) * (i % 2 ? 1.0f : -1.0f);
        g_sprites[i].vy = (14.0f + i * 1.5f) * (i % 3 ? 1.0f : -1.0f);
        g_sprites[i].size = (i % 3 == 0) ? 44.0f : 52.0f;
        g_sprites[i].r = colors[i][0];
        g_sprites[i].g = colors[i][1];
        g_sprites[i].b = colors[i][2];
        g_sprites[i].phase = i * 0.7f;
    }
}

static void update_sprites(float dt) {
    for(int i = 0; i < 10; i++) {
        SpriteObj *sprite = &g_sprites[i];
        float half_size = sprite->size * 0.5f;

        sprite->x += sprite->vx * dt;
        sprite->y += sprite->vy * dt;

        if(sprite->x < half_size || sprite->x > TEX_W - half_size) {
            sprite->vx = -sprite->vx;
            sprite->x += sprite->vx * dt;
        }

        if(sprite->y < half_size || sprite->y > TEX_H - half_size) {
            sprite->vy = -sprite->vy;
            sprite->y += sprite->vy * dt;
        }
    }
}

#ifdef _arch_dreamcast
static int start_pressed(void) {
    maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *state = cont ? (cont_state_t *)maple_dev_status(cont) : NULL;
    return state && (state->buttons & CONT_START);
}
#endif

int main(int argc, char **argv) {
    int frame = 0;
    float dt = 0.016f;

    (void)argc;
    (void)argv;

    glKosInit();
    setup_2d_state();
    load_sprite_texture();

    glGenTextures(1, &g_portal_texture);
    glBindTexture(GL_TEXTURE_2D, g_portal_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, TEX_W, TEX_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    check_gl("create portal texture");

    glGenFramebuffersEXT(1, &g_framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, g_portal_texture, 0);
    
    {
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if(status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            printf("FBO incomplete: 0x%04x\n", (unsigned)status);
            if(status == GL_FRAMEBUFFER_UNSUPPORTED_EXT) {
                printf("RTT framebuffer support requires KOS > 2.3.0\n");
            }
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            return 1;
        }
        printf("FBO created successfully\n");
    }
    
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("create framebuffer");

    init_sprites();

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) {
            break;
        }
#endif

        g_time += dt;
        update_sprites(dt);

        render_portal_texture();
        render_main_scene();

        glKosSwapBuffers();

        if((frame % 60) == 0) {
            printf("[simplesmile] frame %d time %.2f\n", frame, g_time);
        }

        frame++;
    }

    glDeleteFramebuffersEXT(1, &g_framebuffer);
    glDeleteTextures(1, &g_portal_texture);
    glDeleteTextures(1, &g_sprite_texture);

    return 0;
}
