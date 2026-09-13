/*
   fbo_bloom_bmp_demo.c

   GLdc Dreamcast texture-backed FBO / RTT bloom-style demo using the same
   ImageLoad()/loadbmp.h path as the fbo_box_shuffle sample.

   What this demonstrates:
     1. Load a BMP into a normal GL texture.
     2. Render that image into an offscreen GLdc texture-backed FBO / RTT target.
     3. Draw the captured RTT texture normally on the left.
     4. Draw the same captured RTT texture on the right with additive offset
        passes to fake a fixed-function bloom/glow effect.

   This is not shader/HDR bloom. It is a Dreamcast-friendly multipass glow:
     source image -> RTT texture-backed FBO -> final pass -> additive copies

   Asset:
     Put bloom_source_512x256.bmp in this sample's romdisk.

   Recommended asset size:
     512x256 RGB BMP.

   I recommend POT here rather than relying on arbitrary NPOT texture sizes.
   The current RTT path promotes the FBO attachment to a physical RTT backing
   anyway, and 512x256 keeps this demo simple, predictable, and hardware-safe.

   Press START on Dreamcast to exit.
*/

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

#define FBO_W 512
#define FBO_H 256

#define SOURCE_BMP_W FBO_W
#define SOURCE_BMP_H FBO_H

#define NORMAL_X 48.0f
#define NORMAL_Y 112.0f
#define NORMAL_W 224.0f
#define NORMAL_H 224.0f

#define BLOOM_X 368.0f
#define BLOOM_Y 112.0f
#define BLOOM_W 224.0f
#define BLOOM_H 224.0f

#ifdef _arch_dreamcast
#define BLOOM_BMP_PATH "/rd/bloom.bmp"
#else
#define BLOOM_BMP_PATH "samples/fbo_bloom/romdisk/bloom.bmp"
#endif

static GLuint g_source_texture = 0;
static GLuint g_rtt_texture = 0;
static GLuint g_fbo = 0;

static int g_frame = 0;
static float g_pulse = 0.0f;

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
    glDisable(GL_BLEND);
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
                               float v1,
                               GLubyte alpha)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4ub(255, 255, 255, alpha);

    /*
     * Match the stable RTT samples by submitting explicit triangles instead of
     * relying on the quad expansion path for large textured rects.
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

static int load_source_texture(void)
{
    Image image;

    if(!ImageLoad((char*)BLOOM_BMP_PATH, &image)) {
        printf("Failed to load %s\n", BLOOM_BMP_PATH);
        return 0;
    }

    if(image.sizeX != SOURCE_BMP_W || image.sizeY != SOURCE_BMP_H) {
        printf("Unexpected bloom_source.bmp size: %ux%u, expected %ux%u\n",
               image.sizeX, image.sizeY,
               SOURCE_BMP_W, SOURCE_BMP_H);
        free(image.data);
        return 0;
    }

    /*
     * The shared sample BMP loader reads BMP rows in file order. BMP files are
     * bottom-up, while this sample treats texture coordinate v=0 as the top of
     * the image, so flip rows before upload.
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

static int create_rtt_texture_and_fbo(void)
{
    /*
     * RTT target texture. Keep this RGB565 for the current GLdc RTT promotion
     * path. This texture is the FBO color attachment and becomes the offscreen
     * render target.
     */
    glGenTextures(1, &g_rtt_texture);
    glBindTexture(GL_TEXTURE_2D, g_rtt_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS,
                 FBO_W, FBO_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    check_gl("create rtt texture");

    glGenFramebuffersEXT(1, &g_fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D,
                              g_rtt_texture,
                              0);

    {
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        printf("FBO status=0x%04x\n", (unsigned)status);

        if(status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            if(status == GL_FRAMEBUFFER_UNSUPPORTED_EXT) {
                printf("RTT framebuffer support requires KOS > 2.3.0\n");
            }
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            return 0;
        }
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("create fbo");

    return 1;
}

/*
 * First pass:
 * Render the loaded BMP texture into the offscreen RTT/FBO.
 *
 * The animated bright strips are deliberately drawn into the RTT too, so it is
 * obvious the final bloom image is coming from a live captured texture and not
 * just the original BMP drawn directly to the screen.
 */
static void render_source_to_fbo(void)
{
    float marker_x = 40.0f + g_pulse * 380.0f;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_fbo);
    /*
     * Direct RTT rendering uses the attached texture as the target, so the
     * capture pass should render from the texture origin.
     */
    glViewport(0, 0, FBO_W, FBO_H);
    setup_2d_state();
    setup_ortho(FBO_W, FBO_H);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_textured_rect(g_source_texture,
                       0.0f, 0.0f,
                       (float)FBO_W, (float)FBO_H,
                       0.0f, 0.0f,
                       1.0f, 1.0f,
                       255);

    /*
     * Bright animated content captured into the RTT, useful for showing the
     * bloom pass changing live.
     */
    draw_solid_rect(marker_x, 26.0f, 46.0f, 12.0f, 255, 255, 200);
    draw_solid_rect(marker_x + 10.0f, 42.0f, 24.0f, 8.0f, 255, 96, 32);
    draw_solid_rect(210.0f, 202.0f, 92.0f, 8.0f, 180, 240, 255);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("render source to fbo");
}

static void draw_bloom_stack(GLuint texture,
                             float x,
                             float y,
                             float w,
                             float h)
{
    static const int offsets[][2] = {
        { -2,  0 }, {  2,  0 }, {  0, -2 }, {  0,  2 },
        { -3, -3 }, {  3, -3 }, { -3,  3 }, {  3,  3 },
        { -6,  0 }, {  6,  0 }, {  0, -6 }, {  0,  6 },
        { -10, -5 }, {  10, -5 }, { -10,  5 }, {  10,  5 }
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    /*
     * Wide dim copies first, then tighter brighter copies.
     * This gives a cheap soft glow without shaders.
     */
    for(unsigned int i = 8; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        draw_textured_rect(texture,
                           x + (float)offsets[i][0],
                           y + (float)offsets[i][1],
                           w, h,
                           0.0f, 1.0f,
                           1.0f, 0.0f,
                           14);
    }

    for(unsigned int i = 0; i < 8; ++i) {
        draw_textured_rect(texture,
                           x + (float)offsets[i][0],
                           y + (float)offsets[i][1],
                           w, h,
                           0.0f, 1.0f,
                           1.0f, 0.0f,
                           28);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

/*
 * Second pass:
 * Draw the captured RTT texture back to the normal framebuffer.
 *
 * Left panel is the captured texture normally.
 * Right panel is the same captured texture plus additive multi-pass glow.
 */
static void draw_final_scene(void)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    setup_2d_state();
    setup_ortho(SCREEN_W, SCREEN_H);

    glClearColor(0.02f, 0.02f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_solid_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, 4, 4, 8);

    /* Dark panels. */
    draw_solid_rect(16.0f, 56.0f, 304.0f, 264.0f, 18, 18, 24);
    draw_solid_rect(320.0f, 56.0f, 304.0f, 264.0f, 18, 18, 24);

    /* Label bars instead of requiring font support. */
    draw_solid_rect(80.0f, 76.0f, 176.0f, 10.0f, 80, 80, 100);
    draw_solid_rect(392.0f, 76.0f, 176.0f, 10.0f, 180, 150, 80);

    /* The top-down screen projection maps the OpenGL texture top (V=1) to
     * the quad's top edge. GLdc handles the PVR RTT storage orientation. */
    /* Left: captured RTT texture, no bloom. */
    draw_textured_rect(g_rtt_texture,
                       NORMAL_X, NORMAL_Y,
                       NORMAL_W, NORMAL_H,
                       0.0f, 1.0f,
                       1.0f, 0.0f,
                       255);

    /* Right: same captured RTT texture with fake bloom. */
    draw_textured_rect(g_rtt_texture,
                       BLOOM_X, BLOOM_Y,
                       BLOOM_W, BLOOM_H,
                       0.0f, 1.0f,
                       1.0f, 0.0f,
                       255);

    draw_bloom_stack(g_rtt_texture,
                     BLOOM_X,
                     BLOOM_Y,
                     BLOOM_W,
                     BLOOM_H);

    /* Frames. */
    draw_solid_rect(NORMAL_X - 4.0f, NORMAL_Y - 4.0f, NORMAL_W + 8.0f, 4.0f, 70, 70, 90);
    draw_solid_rect(NORMAL_X - 4.0f, NORMAL_Y + NORMAL_H, NORMAL_W + 8.0f, 4.0f, 70, 70, 90);
    draw_solid_rect(NORMAL_X - 4.0f, NORMAL_Y, 4.0f, NORMAL_H, 70, 70, 90);
    draw_solid_rect(NORMAL_X + NORMAL_W, NORMAL_Y, 4.0f, NORMAL_H, 70, 70, 90);

    draw_solid_rect(BLOOM_X - 4.0f, BLOOM_Y - 4.0f, BLOOM_W + 8.0f, 4.0f, 110, 90, 40);
    draw_solid_rect(BLOOM_X - 4.0f, BLOOM_Y + BLOOM_H, BLOOM_W + 8.0f, 4.0f, 110, 90, 40);
    draw_solid_rect(BLOOM_X - 4.0f, BLOOM_Y, 4.0f, BLOOM_H, 110, 90, 40);
    draw_solid_rect(BLOOM_X + BLOOM_W, BLOOM_Y, 4.0f, BLOOM_H, 110, 90, 40);

    check_gl("draw final scene");
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
    (void)argc;
    (void)argv;

    glKosInit();

    if(!load_source_texture()) {
        return 1;
    }

    if(!create_rtt_texture_and_fbo()) {
        return 1;
    }

    printf("fbo_bloom_bmp_demo: BMP -> texture-backed FBO/RTT -> additive bloom pass\n");

    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) {
            break;
        }
#endif

        /*
         * 0..1 ping-pong value without needing sinf/cosf.
         */
        {
            int phase = g_frame % 240;
            if(phase > 120) {
                phase = 240 - phase;
            }
            g_pulse = (float)phase / 120.0f;
        }

        render_source_to_fbo();
        draw_final_scene();
        glKosSwapBuffers();

        if((g_frame % 60) == 0) {
            printf("[bloom] frame %d time_ms=%llu\n",
                   g_frame,
                   (unsigned long long)now_ms());
        }

        ++g_frame;
    }

    return 0;
}
