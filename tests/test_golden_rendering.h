#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"
#include "tools/golden.h"

#include <stdint.h>
#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/private.h"

/* =========================================================================
 * GoldenRenderingTests
 *
 * End-to-end "golden image" tests. Each test issues real GL draw calls, then
 * rasterises the resulting TA poly-lists with the deterministic CPU rasteriser
 * in tools/golden.h and compares the output to a committed PPM reference under
 * tests/goldens/.
 *
 * These guard the whole submission pipeline — vertex transform, viewport
 * mapping, colour/UV handling, primitive assembly, list routing (opaque vs
 * transparent) and texture sampling — against regressions.
 *
 * To (re)generate the references run the test binary with GLDC_UPDATE_GOLDENS=1.
 * =========================================================================*/
class GoldenRenderingTests : public GLTestCase {
public:
    /* The video mode is 640x480 and the viewport y-flip is relative to that
     * height, so to capture a WxH image at the top-left of the framebuffer we
     * place the viewport at (0, 480 - H). */
    static const int VIDEO_H = 480;
    static const int W = 96;
    static const int H = 96;

    void set_up() {
        GLTestCase::set_up();
        glViewport(0, VIDEO_H - H, W, H);
    }

    /* A single white triangle on a black background. */
    void test_solid_white_triangle() {
        golden::Image img(W, H);
        img.clear(0, 0, 0);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.8f, -0.8f, 0.0f);
            glVertex3f( 0.8f, -0.8f, 0.0f);
            glVertex3f( 0.0f,  0.8f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "solid_white_triangle"));
    }

    /* Gouraud-shaded triangle: red/green/blue corners interpolated. */
    void test_gouraud_triangle() {
        golden::Image img(W, H);
        img.clear(0, 0, 0);

        glBegin(GL_TRIANGLES);
            glColor4f(1.0f, 0.0f, 0.0f, 1.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
            glColor4f(0.0f, 1.0f, 0.0f, 1.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
            glColor4f(0.0f, 0.0f, 1.0f, 1.0f); glVertex3f( 0.0f,  0.8f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "gouraud_triangle"));
    }

    /* A full-screen-ish coloured quad built from a triangle strip. */
    void test_colored_quad_strip() {
        golden::Image img(W, H);
        img.clear(16, 16, 16);

        glBegin(GL_TRIANGLE_STRIP);
            glColor4f(1.0f, 0.0f, 0.0f, 1.0f); glVertex3f(-0.7f,  0.7f, 0.0f);
            glColor4f(1.0f, 1.0f, 0.0f, 1.0f); glVertex3f(-0.7f, -0.7f, 0.0f);
            glColor4f(0.0f, 1.0f, 0.0f, 1.0f); glVertex3f( 0.7f,  0.7f, 0.0f);
            glColor4f(0.0f, 0.0f, 1.0f, 1.0f); glVertex3f( 0.7f, -0.7f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "colored_quad_strip"));
    }

    /* Two overlapping opaque triangles: painter's order must put the second
     * (green) on top of the first (red). */
    void test_overlapping_opaque_painter_order() {
        golden::Image img(W, H);
        img.clear(0, 0, 0);

        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.8f, -0.6f, 0.0f);
            glVertex3f( 0.4f, -0.6f, 0.0f);
            glVertex3f(-0.2f,  0.8f, 0.0f);
        glEnd();

        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.4f, -0.8f, 0.0f);
            glVertex3f( 0.8f, -0.8f, 0.0f);
            glVertex3f( 0.2f,  0.6f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "overlapping_opaque"));
    }

    /* A half-transparent quad over an opaque red triangle: routed to the
     * transparent list and alpha-blended by the rasteriser. */
    void test_transparent_over_opaque() {
        golden::Image img(W, H);
        img.clear(0, 0, 0);

        /* Opaque red triangle (OP_LIST). */
        glDisable(GL_BLEND);
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.8f, -0.8f, 0.0f);
            glVertex3f( 0.8f, -0.8f, 0.0f);
            glVertex3f( 0.0f,  0.8f, 0.0f);
        glEnd();

        /* 50% blue quad on top (TR_LIST). */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
        glBegin(GL_TRIANGLE_STRIP);
            glVertex3f(-0.5f,  0.5f, 0.0f);
            glVertex3f(-0.5f, -0.5f, 0.0f);
            glVertex3f( 0.5f,  0.5f, 0.0f);
            glVertex3f( 0.5f, -0.5f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "transparent_over_opaque"));
    }

    /* A Gouraud quad coloured via a GL_BGRA GL_UNSIGNED_BYTE array (OpenGL 1.4),
     * blended over a coloured background. This exercises the whole BGRA path end
     * to end: the red/blue channel swap must land the right colour at each
     * corner, and the per-vertex alpha must drive the blend against the
     * background. If BGRA decoded as RGBA, or alpha were ignored, the rendered
     * output would differ from the golden. */
    void test_bgra_vertex_colors_with_alpha() {
        golden::Image img(W, H);
        img.clear(40, 40, 40);  /* dark grey so blending is visible */

        GLfloat verts[] = {
            -0.8f,  0.8f, 0.0f,
            -0.8f, -0.8f, 0.0f,
             0.8f,  0.8f, 0.0f,
             0.8f, -0.8f, 0.0f,
        };
        /* GL_BGRA memory order is B, G, R, A -> colour R, G, B, A.
         * Distinct corners with decreasing alpha down/right. */
        GLubyte colors[] = {
            /*  B    G    R    A        -> colour              */
                0,   0, 255, 255,    /* opaque red            */
                0, 255,   0, 192,    /* green,  ~75% alpha    */
              255,   0,   0, 128,    /* blue,    50% alpha    */
                0, 255, 255,  64,    /* yellow,  25% alpha    */
        };

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, 0, colors);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "bgra_vertex_colors_alpha"));
    }

    /* A textured quad. A 2x2 RGB texture (red/green/blue/white) is mapped over
     * a quad; nearest filtering gives four solid colour cells. */
    void test_textured_quad() {
        golden::Image img(W, H);
        img.clear(0, 0, 0);

        /* 8x8 (the PVR minimum) split into four 4x4 colour quadrants:
         * red / green / blue / white. */
        GLubyte texels[8 * 8 * 3];
        for(int y = 0; y < 8; ++y) {
            for(int x = 0; x < 8; ++x) {
                GLubyte r = 0, g = 0, b = 0;
                if(y < 4 && x < 4)      { r = 255; }                 /* top-left red */
                else if(y < 4)          { g = 255; }                 /* top-right green */
                else if(x < 4)          { b = 255; }                 /* bottom-left blue */
                else                    { r = g = b = 255; }         /* bottom-right white */
                int i = (y * 8 + x) * 3;
                texels[i + 0] = r; texels[i + 1] = g; texels[i + 2] = b;
            }
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, texels);
        assert_equal(glGetError(), GL_NO_ERROR);

        glEnable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img, _glGetBoundTexture());
        assert_true(golden::check(img, "textured_quad"));

        glDeleteTextures(1, &tex);
    }

    /* The same triangle drawn with a coloured background must leave the
     * background untouched outside the triangle (clear-colour preservation). */
    void test_triangle_preserves_background() {
        golden::Image img(W, H);
        img.clear(40, 80, 120);

        glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.5f, -0.5f, 0.0f);
            glVertex3f( 0.5f, -0.5f, 0.0f);
            glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();

        golden::rasterize_all_lists(img);
        assert_true(golden::check(img, "triangle_on_background"));
    }
};
