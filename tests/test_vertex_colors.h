#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <stdint.h>
#include <vector>
#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/private.h"
#include "GL/state.h"
#include "containers/aligned_vector.h"

/* =========================================================================
 * VertexColorTests
 *
 * Byte vertex-colour coverage for glColorPointer, with particular attention to
 * GL_BGRA as specified by OpenGL 1.4.
 *
 * Per the spec, a colour array may use size = GL_BGRA (only valid with
 * GL_UNSIGNED_BYTE). The four bytes are then stored in memory in B, G, R, A
 * order but are interpreted as the R, G, B, A colour components — i.e. the
 * red and blue channels are swapped relative to a normal GL_RGBA array. Alpha
 * stays in the last byte.
 *
 * GLdc stores the resulting colour per vertex as normalised floats in ARGB
 * slot order (A8IDX / R8IDX / G8IDX / B8IDX). These tests draw a triangle and
 * read the submitted vertices back out of OP_LIST to verify the byte->float
 * conversion and the BGRA channel ordering.
 * =========================================================================*/
class VertexColorTests : public GLTestCase {
public:
    static const GLfloat* positions() {
        static const GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             0.0f,  1.0f, 0.5f,
        };
        return verts;
    }

    /* Submitted (non-header) vertices currently in OP_LIST. */
    static std::vector<Vertex> captured() {
        std::vector<Vertex> out;
        uint32_t n = aligned_vector_size(&OP_LIST.vector);
        for(uint32_t i = 0; i < n; ++i) {
            Vertex* v = (Vertex*) aligned_vector_at(&OP_LIST.vector, i);
            if(v->flags == GPU_CMD_VERTEX || v->flags == GPU_CMD_VERTEX_EOL) {
                out.push_back(*v);
            }
        }
        return out;
    }

    /* Draw a single triangle whose vertices use the supplied colour array. */
    std::vector<Vertex> draw_with_colors(GLint size, GLenum type, const void* colors) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glColorPointer(size, type, 0, colors);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        return captured();
    }

    void assert_argb_close(const Vertex& v, float r, float g, float b, float a) {
        assert_close(v.argb[R8IDX], r, 0.005f);
        assert_close(v.argb[G8IDX], g, 0.005f);
        assert_close(v.argb[B8IDX], b, 0.005f);
        assert_close(v.argb[A8IDX], a, 0.005f);
    }

    /* --------------------------------------------------- byte RGBA values */

    /* size=4 GL_UNSIGNED_BYTE: each byte is normalised by 1/255 and kept in
     * RGBA order. Use non-trivial values so a wrong scale or order shows up. */
    void test_byte_rgba4_normalizes_and_keeps_order() {
        GLubyte colors[] = {
             10,  20,  30,  40,
            200, 150, 100,  50,
            255, 128,   1, 254,
        };
        std::vector<Vertex> v = draw_with_colors(4, GL_UNSIGNED_BYTE, colors);
        assert_equal(v.size(), (size_t) 3);
        assert_argb_close(v[0],  10/255.0f,  20/255.0f,  30/255.0f,  40/255.0f);
        assert_argb_close(v[1], 200/255.0f, 150/255.0f, 100/255.0f,  50/255.0f);
        assert_argb_close(v[2], 255/255.0f, 128/255.0f,   1/255.0f, 254/255.0f);
    }

    /* size=3 GL_UNSIGNED_BYTE: RGB normalised, alpha forced to 1.0. */
    void test_byte_rgb3_forces_alpha_one() {
        GLubyte colors[] = {
             10,  20,  30,
            200, 150, 100,
              0,   0,   0,
        };
        std::vector<Vertex> v = draw_with_colors(3, GL_UNSIGNED_BYTE, colors);
        assert_equal(v.size(), (size_t) 3);
        assert_argb_close(v[0],  10/255.0f,  20/255.0f,  30/255.0f, 1.0f);
        assert_argb_close(v[1], 200/255.0f, 150/255.0f, 100/255.0f, 1.0f);
        assert_argb_close(v[2],   0.0f,       0.0f,       0.0f,      1.0f);
    }

    /* --------------------------------------------------- GL_BGRA (GL 1.4) */

    /* GL_BGRA: memory order is B, G, R, A but maps to R, G, B, A. With distinct
     * channel values this proves the red/blue swap (and that alpha is the last
     * byte). */
    void test_bgra_byte_channel_order() {
        /* memory: B=10, G=20, R=30, A=200  ->  colour R=30, G=20, B=10, A=200 */
        GLubyte colors[] = {
            10, 20, 30, 200,
            10, 20, 30, 200,
            10, 20, 30, 200,
        };
        std::vector<Vertex> v = draw_with_colors(GL_BGRA, GL_UNSIGNED_BYTE, colors);
        assert_equal(v.size(), (size_t) 3);
        for(size_t i = 0; i < v.size(); ++i) {
            assert_argb_close(v[i], 30/255.0f, 20/255.0f, 10/255.0f, 200/255.0f);
        }
    }

    /* A GL_BGRA array laid out as {B,G,R,A} must produce exactly the same
     * submitted colour as the equivalent GL_RGBA array {R,G,B,A}. */
    void test_bgra_matches_equivalent_rgba() {
        /* Logical colour: R=30, G=20, B=10, A=200 */
        GLubyte rgba[] = {
            30, 20, 10, 200,
            30, 20, 10, 200,
            30, 20, 10, 200,
        };
        GLubyte bgra[] = {
            10, 20, 30, 200,
            10, 20, 30, 200,
            10, 20, 30, 200,
        };

        std::vector<Vertex> rgba_v = draw_with_colors(4, GL_UNSIGNED_BYTE, rgba);

        /* Fresh list for the BGRA draw. */
        aligned_vector_clear(&OP_LIST.vector);
        _glGPUStateMarkDirty();
        std::vector<Vertex> bgra_v = draw_with_colors(GL_BGRA, GL_UNSIGNED_BYTE, bgra);

        assert_equal(rgba_v.size(), bgra_v.size());
        for(size_t i = 0; i < rgba_v.size(); ++i) {
            assert_close(rgba_v[i].argb[R8IDX], bgra_v[i].argb[R8IDX], 0.005f);
            assert_close(rgba_v[i].argb[G8IDX], bgra_v[i].argb[G8IDX], 0.005f);
            assert_close(rgba_v[i].argb[B8IDX], bgra_v[i].argb[B8IDX], 0.005f);
            assert_close(rgba_v[i].argb[A8IDX], bgra_v[i].argb[A8IDX], 0.005f);
        }
    }

    /* Distinct per-vertex GL_BGRA colours must each map correctly. */
    void test_bgra_per_vertex_distinct() {
        GLubyte colors[] = {
            /* B    G    R    A   ->  R / G / B / A */
              0,   0, 255, 255,   /* red,   opaque  */
              0, 255,   0, 128,   /* green, half a  */
            255,   0,   0,  64,   /* blue,  quarter */
        };
        std::vector<Vertex> v = draw_with_colors(GL_BGRA, GL_UNSIGNED_BYTE, colors);
        assert_equal(v.size(), (size_t) 3);
        assert_argb_close(v[0], 1.0f, 0.0f, 0.0f, 255/255.0f);  /* red   */
        assert_argb_close(v[1], 0.0f, 1.0f, 0.0f, 128/255.0f);  /* green */
        assert_argb_close(v[2], 0.0f, 0.0f, 1.0f,  64/255.0f);  /* blue  */
    }

    /* Alpha for GL_BGRA comes from the last byte, independent of the RGB
     * channels. */
    void test_bgra_alpha_is_independent() {
        GLubyte colors[] = {
            255, 255, 255,   0,   /* white, fully transparent */
            255, 255, 255,   0,
            255, 255, 255,   0,
        };
        std::vector<Vertex> v = draw_with_colors(GL_BGRA, GL_UNSIGNED_BYTE, colors);
        assert_equal(v.size(), (size_t) 3);
        for(size_t i = 0; i < v.size(); ++i) {
            assert_argb_close(v[i], 1.0f, 1.0f, 1.0f, 0.0f);
        }
    }

    /* GL_BGRA is accepted by glColorPointer without raising an error. */
    void test_bgra_size_is_accepted() {
        GLubyte colors[] = { 1, 2, 3, 4 };
        glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(glGetError(), GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size, (GLint) GL_BGRA);
    }
};
