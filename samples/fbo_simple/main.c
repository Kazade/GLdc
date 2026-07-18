#include <stdint.h>
#include <stdio.h>
#include <math.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#define TEX_W 128
#define TEX_H 128
#define WIN_W  640
#define WIN_H  480

#define PORTAL_X 220
#define PORTAL_Y 140
#define PORTAL_W 200
#define PORTAL_H 200

static GLuint g_portal_texture = 0;
static GLuint g_framebuffer = 0;

// Scene objects
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float size;
    float r, g, b;
    float rotation;
} Object3D;

static Object3D g_objects[8];
static float g_time = 0.0f;

static void check_gl(const char* where) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        printf("[GL] %s: error 0x%04x\n", where, (unsigned)err);
    }
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

// Helper for alpha-blended rects
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

static void draw_textured_rect(GLuint texture,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1) {
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

// Draw 2D representation of 3D objects (since we're in 2D mode for RTT)
static void draw_2d_scene_to_texture(void) {
    // Dark background
    draw_solid_rect(0, 0, TEX_W, TEX_H, 8, 12, 24);
    
    // Draw a grid pattern
    for(int i = 0; i < TEX_W; i += 8) {
        draw_solid_rect(i, 0, 1, TEX_H, 30, 30, 50);
        draw_solid_rect(0, i, TEX_W, 1, 30, 30, 50);
    }
    
    // Draw floating objects as colored rectangles
    for(int i = 0; i < 8; i++) {
        Object3D *obj = &g_objects[i];
        float x = obj->x * 12 + TEX_W/2;
        float y = obj->y * 12 + TEX_H/2;
        float size = obj->size * 20 + 8;
        
        // Draw with some 3D perspective illusion
        draw_solid_rect(x - size/2, y - size/2, size, size, 
                       (GLubyte)(obj->r * 255), 
                       (GLubyte)(obj->g * 255), 
                       (GLubyte)(obj->b * 255));
        
        // Add a highlight
        draw_solid_rect(x - size/4, y - size/4, size/4, size/4,
                       255, 255, 255);
    }
    
    // Draw a rotating ring
    float cx = TEX_W/2;
    float cy = TEX_H/2;
    for(int i = 0; i < 20; i++) {
        float angle = g_time + i * 0.3f;
        float radius = 20 + sin(g_time * 0.5f + i) * 5;
        float x = cx + cos(angle) * radius;
        float y = cy + sin(angle * 0.5f + g_time) * radius;
        draw_solid_rect(x - 2, y - 2, 4, 4, 150, 50, 255);
    }
    
    // Draw a moving "player" like the original example
    float player_x = (sin(g_time * 0.5f) * 0.5f + 0.5f) * (TEX_W - 20);
    draw_solid_rect(player_x, 20, 20, 20, 255, 0, 0);
    draw_solid_rect(player_x + 5, 28, 10, 8, 0, 255, 0);
}

// Render the portal scene to texture using direct RTT.
static void render_portal_texture(void) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_framebuffer);

    /*
     * Direct RTT renders offscreen into the attached texture backing.
     * Use the logical target viewport; no old fake-FBO capture offset.
     */
    glViewport(0, 0, TEX_W, TEX_H);
    setup_2d_state();
    setup_ortho(TEX_W, TEX_H);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the 2D scene to texture
    draw_2d_scene_to_texture();

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("render portal texture");
}


// Main scene with the portal
static void render_main_scene(void) {
    glViewport(0, 0, WIN_W, WIN_H);
    setup_2d_state();
    setup_ortho(WIN_W, WIN_H);
    
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw background
    draw_solid_rect(0, 0, WIN_W, WIN_H, 10, 10, 20);
    
    // Decorative borders
    draw_solid_rect(0, 0, WIN_W, 20, 40, 30, 60);
    draw_solid_rect(0, WIN_H-20, WIN_W, 20, 40, 30, 60);
    draw_solid_rect(0, 0, 20, WIN_H, 40, 30, 60);
    draw_solid_rect(WIN_W-20, 0, 20, WIN_H, 40, 30, 60);
    
    // Title
    draw_solid_rect(230, 10, 180, 20, 100, 80, 200);
    
    // Portal glow
    float border = 8;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    draw_solid_rect_alpha(PORTAL_X - border*2, PORTAL_Y - border*2, 
                         PORTAL_W + border*4, PORTAL_H + border*4, 
                         30, 10, 60, 100);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Portal border
    draw_solid_rect(PORTAL_X - border, PORTAL_Y - border,
                    PORTAL_W + border*2, border, 80, 50, 150);
    draw_solid_rect(PORTAL_X - border, PORTAL_Y + PORTAL_H,
                    PORTAL_W + border*2, border, 80, 50, 150);
    draw_solid_rect(PORTAL_X - border, PORTAL_Y,
                    border, PORTAL_H, 80, 50, 150);
    draw_solid_rect(PORTAL_X + PORTAL_W, PORTAL_Y,
                    border, PORTAL_H, 80, 50, 150);
    
    // Draw the portal texture
    /*
     * PVR RTT Y-flip: pvr_scene_begin_rtt writes framebuffer y=0 to the top
     * row of texture memory (v=0), but OpenGL texcoord v=0 is the bottom. Flip
     * V to compensate.
     */
    draw_textured_rect(g_portal_texture,
                       PORTAL_X, PORTAL_Y,
                       PORTAL_W, PORTAL_H,
                       0, 1, 1, 0);
    
    // Portal vignette
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_solid_rect_alpha(PORTAL_X, PORTAL_Y, PORTAL_W, PORTAL_H, 0, 0, 0, 80);
    
    // Floating particles around portal
    for(int i = 0; i < 20; i++) {
        float angle = g_time + i * 1.2f;
        float dist = 1.0f + sin(g_time * 0.5f + i) * 0.3f;
        float px = PORTAL_X + PORTAL_W/2 + cos(angle) * dist * PORTAL_W/2 * 0.6f;
        float py = PORTAL_Y + PORTAL_H/2 + sin(angle * 0.7f + g_time) * dist * PORTAL_H/2 * 0.6f;
        float size = 3 + sin(g_time + i) * 2;
        int color = i % 3;
        if(color == 0) draw_solid_rect(px, py, size, size, 150, 50, 255);
        else if(color == 1) draw_solid_rect(px, py, size, size, 50, 150, 255);
        else draw_solid_rect(px, py, size, size, 255, 50, 150);
    }
    
    glDisable(GL_BLEND);
}

static void init_objects(void) {
    float colors[8][3] = {
        {1.0f, 0.2f, 0.2f},
        {0.2f, 1.0f, 0.2f},
        {0.2f, 0.2f, 1.0f},
        {1.0f, 1.0f, 0.2f},
        {1.0f, 0.2f, 1.0f},
        {0.2f, 1.0f, 1.0f},
        {1.0f, 0.5f, 0.2f},
        {0.5f, 0.2f, 1.0f}
    };
    
    for(int i = 0; i < 8; i++) {
        g_objects[i].x = (i % 3 - 1) * 1.2f;
        g_objects[i].y = (i / 3 - 1) * 1.0f;
        g_objects[i].z = 0;
        g_objects[i].vx = (i * 0.2f + 0.1f) * (i % 2 ? 1 : -1);
        g_objects[i].vy = (i * 0.15f + 0.1f) * (i % 3 ? 1 : -1);
        g_objects[i].vz = 0;
        g_objects[i].size = 0.3f + i * 0.05f;
        g_objects[i].r = colors[i][0];
        g_objects[i].g = colors[i][1];
        g_objects[i].b = colors[i][2];
        g_objects[i].rotation = i * 45.0f;
    }
}

static void update_objects(float dt) {
    for(int i = 0; i < 8; i++) {
        Object3D *obj = &g_objects[i];
        obj->x += obj->vx * dt;
        obj->y += obj->vy * dt;
        
        // Bounce in 2D
        float half_range = 2.5f;
        if(obj->x > half_range || obj->x < -half_range) obj->vx = -obj->vx;
        if(obj->y > half_range || obj->y < -half_range) obj->vy = -obj->vy;
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
    
    // Create portal texture
    glGenTextures(1, &g_portal_texture);
    glBindTexture(GL_TEXTURE_2D, g_portal_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, TEX_W, TEX_H, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    check_gl("create portal texture");
    
    // Create framebuffer and attach the portal texture once.
    glGenFramebuffersEXT(1, &g_framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, g_portal_texture, 0);
    {
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        printf("FBO status=0x%04x\n", (unsigned)status);
        if(status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            if(status == GL_FRAMEBUFFER_UNSUPPORTED_EXT) {
                printf("RTT framebuffer support requires KOS > 2.3.0\n");
            }
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            return 1;
        }
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    check_gl("create framebuffer");
    
    init_objects();
    
    while(1) {
#ifdef _arch_dreamcast
        if(start_pressed()) {
            break;
        }
#endif
        
        g_time += dt;
        update_objects(dt);
        
        render_portal_texture();
        render_main_scene();
        
        glKosSwapBuffers();
        
        if((frame % 60) == 0) {
            printf("[portal] frame %d time %.2f\n", frame, g_time);
        }
        
        frame++;
    }
    
    glDeleteFramebuffersEXT(1, &g_framebuffer);
    glDeleteTextures(1, &g_portal_texture);
    
    return 0;
}
