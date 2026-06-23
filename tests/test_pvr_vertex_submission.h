#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <stdint.h>
#include <GL/gl.h>
#include <GL/glkos.h>

/*
 * Internal headers needed to inspect the Tile Accelerator (TA) submission
 * buffers directly after a draw call.
 *
 * After glDrawArrays / glEnd, the GL driver fills one of three poly-lists
 * (OP_LIST / PT_LIST / TR_LIST).  Each list is an AlignedVector whose
 * elements are 64-byte Vertex structs.  The first element in the vector
 * after a draw is a PolyHeader (also 64 bytes), followed by the actual
 * vertex data.
 *
 * This file tests:
 *   - That the expected number of PolyHeader + Vertex entries appear
 *     in OP_LIST after various draw calls.
 *   - That the GPU_CMD_VERTEX / GPU_CMD_VERTEX_EOL flags are set on
 *     the correct vertices.
 *   - That the PolyHeader command word is valid.
 *   - That primary colour set via glColor* or glColorPointer appears
 *     correctly in each submitted vertex's argb[] field.
 */
#include "GL/private.h"
#include "containers/aligned_vector.h"


/* =========================================================================
 * PVRVertexSubmissionTests
 * =========================================================================*/
class PVRVertexSubmissionTests : public GLTestCase {
public:
    /* GLTestCase::set_up() gives a clean slate: GPU initialised once, colour
     * reset to white and all client-state arrays disabled. */

    /* Convenience: cast element i of a list to Vertex*. */
    static Vertex* vertex_at(PolyList* list, uint32_t i) {
        return (Vertex*) aligned_vector_at(&list->vector, i);
    }

    /* -----------------------------------------------------------------------
     * Basic list structure
     * -------------------------------------------------------------------- */

    /* OP_LIST must be empty before the first draw call. */
    void test_op_list_is_empty_before_any_draw() {
        assert_equal(aligned_vector_size(&OP_LIST.vector), 0u);
    }

    /* Drawing a single triangle via immediate mode must produce exactly
     * one PolyHeader and three vertices (four elements total). */
    void test_immediate_triangle_produces_four_list_entries() {
        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        assert_equal(aligned_vector_size(&OP_LIST.vector), 4u);
    }

    /* A second triangle drawn while GPU state is unchanged must not add
     * another PolyHeader, yielding 4 + 3 = 7 entries. */
    void test_second_triangle_without_state_change_shares_header() {
        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        glBegin(GL_TRIANGLES);
            glVertex3f(-0.5f, -0.5f, 0.5f);
            glVertex3f( 0.5f, -0.5f, 0.5f);
            glVertex3f( 0.0f,  0.5f, 0.5f);
        glEnd();

        /* 1 header + 3 verts + 3 verts = 7 */
        assert_equal(aligned_vector_size(&OP_LIST.vector), 7u);
    }

    /* After glKosSwapBuffers the list must be cleared. */
    void test_op_list_is_empty_after_swap_buffers() {
        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        glKosSwapBuffers();

        assert_equal(aligned_vector_size(&OP_LIST.vector), 0u);
    }

    /* -----------------------------------------------------------------------
     * PolyHeader validation
     * -------------------------------------------------------------------- */

    /* The PolyHeader command word must have the polygon-header type bits
     * set (upper byte = 0x80) as defined by GPU_CMD_POLYHDR = 0x80840000. */
    void test_poly_header_has_correct_upper_byte() {
        glBegin(GL_TRIANGLES);
            glVertex3f(0.0f, 0.0f, 0.5f);
            glVertex3f(1.0f, 0.0f, 0.5f);
            glVertex3f(0.0f, 1.0f, 0.5f);
        glEnd();

        PolyHeader* hdr = (PolyHeader*) aligned_vector_at(&OP_LIST.vector, 0);
        /* Top byte must be 0x80 (polygon-header command). */
        assert_equal(hdr->cmd & 0xFF000000u, 0x80000000u);
    }

    /* For the opaque list the blend word in mode2 must encode ONE/ZERO. */
    void test_opaque_poly_header_blend_is_one_zero() {
        glBegin(GL_TRIANGLES);
            glVertex3f(0.0f, 0.0f, 0.5f);
            glVertex3f(1.0f, 0.0f, 0.5f);
            glVertex3f(0.0f, 1.0f, 0.5f);
        glEnd();

        PolyHeader* hdr = (PolyHeader*) aligned_vector_at(&OP_LIST.vector, 0);

        /* src blend = GPU_BLEND_ONE (1) in bits 31-29 of mode2 */
        uint32_t src = (hdr->mode2 >> GPU_TA_PM2_SRCBLEND_SHIFT) & 0x7;
        /* dst blend = GPU_BLEND_ZERO (0) in bits 28-26 of mode2 */
        uint32_t dst = (hdr->mode2 >> GPU_TA_PM2_DSTBLEND_SHIFT) & 0x7;

        assert_equal(src, (uint32_t)GPU_BLEND_ONE);
        assert_equal(dst, (uint32_t)GPU_BLEND_ZERO);
    }

    /* -----------------------------------------------------------------------
     * Vertex flag tests
     * -------------------------------------------------------------------- */

    /* For GL_TRIANGLES, vertices 0 and 1 of the strip carry GPU_CMD_VERTEX;
     * vertex 2 (end of triangle) carries GPU_CMD_VERTEX_EOL. */
    void test_triangle_vertex_flags_are_set_correctly() {
        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        /* Index 0 is the PolyHeader; vertices start at index 1. */
        Vertex* v0 = vertex_at(&OP_LIST, 1);
        Vertex* v1 = vertex_at(&OP_LIST, 2);
        Vertex* v2 = vertex_at(&OP_LIST, 3);

        assert_equal(v0->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(v1->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(v2->flags, (uint32_t)GPU_CMD_VERTEX_EOL);
    }

    /* For two back-to-back GL_TRIANGLES the EOL flag must appear on every
     * third vertex (indices 3 and 6 within the combined list). */
    void test_two_triangles_have_eol_on_every_third_vertex() {
        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        glBegin(GL_TRIANGLES);
            glVertex3f(-0.5f, -0.5f, 0.5f);
            glVertex3f( 0.5f, -0.5f, 0.5f);
            glVertex3f( 0.0f,  0.5f, 0.5f);
        glEnd();

        /* 1 header + 6 vertices (indices 1..6) */
        assert_equal(vertex_at(&OP_LIST, 1)->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(vertex_at(&OP_LIST, 2)->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(vertex_at(&OP_LIST, 3)->flags, (uint32_t)GPU_CMD_VERTEX_EOL);
        assert_equal(vertex_at(&OP_LIST, 4)->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(vertex_at(&OP_LIST, 5)->flags, (uint32_t)GPU_CMD_VERTEX);
        assert_equal(vertex_at(&OP_LIST, 6)->flags, (uint32_t)GPU_CMD_VERTEX_EOL);
    }

    /* -----------------------------------------------------------------------
     * Immediate-mode colour tests
     * -------------------------------------------------------------------- */

    /* A single colour set before all vertices must appear identically in
     * every submitted vertex (alpha, red, green, blue in ARGB order). */
    void test_immediate_single_color_appears_in_all_vertices() {
        /* Set a distinctive colour: R=1, G=0, B=0, A=1 */
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);

        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        for(uint32_t i = 1; i <= 3; ++i) {
            Vertex* v = vertex_at(&OP_LIST, i);
            assert_close(v->argb[A8IDX], 1.0f, 0.001f);
            assert_close(v->argb[R8IDX], 1.0f, 0.001f);
            assert_close(v->argb[G8IDX], 0.0f, 0.001f);
            assert_close(v->argb[B8IDX], 0.0f, 0.001f);
        }
    }

    /* glColor4ub must also produce the correct normalised float values in
     * the submitted vertices. */
    void test_immediate_color4ub_appears_correctly_in_vertex() {
        glColor4ub(0, 255, 0, 255);  /* green */

        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        Vertex* v = vertex_at(&OP_LIST, 1);
        assert_close(v->argb[A8IDX], 1.0f, 0.005f);
        assert_close(v->argb[R8IDX], 0.0f, 0.005f);
        assert_close(v->argb[G8IDX], 1.0f, 0.005f);
        assert_close(v->argb[B8IDX], 0.0f, 0.005f);
    }

    /* Changing the colour between vertex calls must produce per-vertex
     * colour variation in the submitted list. */
    void test_immediate_per_vertex_colors_are_distinct() {
        glBegin(GL_TRIANGLES);
            glColor4f(1.0f, 0.0f, 0.0f, 1.0f);  /* red   */
            glVertex3f(-1.0f, -1.0f, 0.5f);

            glColor4f(0.0f, 1.0f, 0.0f, 1.0f);  /* green */
            glVertex3f( 1.0f, -1.0f, 0.5f);

            glColor4f(0.0f, 0.0f, 1.0f, 1.0f);  /* blue  */
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        Vertex* v0 = vertex_at(&OP_LIST, 1);   /* red   */
        Vertex* v1 = vertex_at(&OP_LIST, 2);   /* green */
        Vertex* v2 = vertex_at(&OP_LIST, 3);   /* blue  */

        /* v0 = red */
        assert_close(v0->argb[R8IDX], 1.0f, 0.001f);
        assert_close(v0->argb[G8IDX], 0.0f, 0.001f);
        assert_close(v0->argb[B8IDX], 0.0f, 0.001f);
        assert_close(v0->argb[A8IDX], 1.0f, 0.001f);

        /* v1 = green */
        assert_close(v1->argb[R8IDX], 0.0f, 0.001f);
        assert_close(v1->argb[G8IDX], 1.0f, 0.001f);
        assert_close(v1->argb[B8IDX], 0.0f, 0.001f);
        assert_close(v1->argb[A8IDX], 1.0f, 0.001f);

        /* v2 = blue */
        assert_close(v2->argb[R8IDX], 0.0f, 0.001f);
        assert_close(v2->argb[G8IDX], 0.0f, 0.001f);
        assert_close(v2->argb[B8IDX], 1.0f, 0.001f);
        assert_close(v2->argb[A8IDX], 1.0f, 0.001f);
    }

    /* glColor3f (no alpha) must result in alpha=1 in the submitted vertex. */
    void test_immediate_color3f_vertex_has_full_alpha() {
        glColor3f(0.5f, 0.25f, 0.1f);

        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        Vertex* v = vertex_at(&OP_LIST, 1);
        assert_close(v->argb[A8IDX], 1.0f,  0.001f);
        assert_close(v->argb[R8IDX], 0.5f,  0.001f);
        assert_close(v->argb[G8IDX], 0.25f, 0.001f);
        assert_close(v->argb[B8IDX], 0.1f,  0.001f);
    }

    /* -----------------------------------------------------------------------
     * Vertex-array colour tests  (glColorPointer path)
     * -------------------------------------------------------------------- */

    /* When no colour array is active the current colour (set via glColor*)
     * must be used for every submitted vertex. */
    void test_no_color_pointer_uses_current_color_for_all_vertices() {
        glColor4f(0.5f, 0.25f, 0.1f, 0.8f);

        GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             0.0f,  1.0f, 0.5f
        };

        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableClientState(GL_VERTEX_ARRAY);

        Vertex* v = vertex_at(&OP_LIST, 1);
        assert_close(v->argb[R8IDX], 0.5f,  0.001f);
        assert_close(v->argb[G8IDX], 0.25f, 0.001f);
        assert_close(v->argb[B8IDX], 0.1f,  0.001f);
        assert_close(v->argb[A8IDX], 0.8f,  0.001f);
    }

    /* Colours supplied via glColorPointer (GL_FLOAT, size 4, RGBA order)
     * must reach the submitted vertices in ARGB layout.
     *
     * _readColour4fARGB treats the input as RGBA and writes:
     *   argb[R8IDX] = input[0]   argb[G8IDX] = input[1]
     *   argb[B8IDX] = input[2]   argb[A8IDX] = input[3]
     */
    void test_color_pointer_float4_rgba_stored_correctly() {
        GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             0.0f,  1.0f, 0.5f
        };
        /* RGBA float colours: red, green, blue */
        GLfloat colors[] = {
            1.0f, 0.0f, 0.0f, 1.0f,   /* red   */
            0.0f, 1.0f, 0.0f, 1.0f,   /* green */
            0.0f, 0.0f, 1.0f, 1.0f    /* blue  */
        };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        Vertex* v0 = vertex_at(&OP_LIST, 1);   /* red   */
        Vertex* v1 = vertex_at(&OP_LIST, 2);   /* green */
        Vertex* v2 = vertex_at(&OP_LIST, 3);   /* blue  */

        /* v0 = red */
        fprintf(stderr, "%f %f %f %f\n", v0->argb[0], v0->argb[1], v0->argb[2], v0->argb[3]);
        assert_close(v0->argb[0], 1.0f, 0.001f);
        assert_close(v0->argb[1], 0.0f, 0.001f);
        assert_close(v0->argb[2], 0.0f, 0.001f);
        assert_close(v0->argb[3], 1.0f, 0.001f);

        /* v1 = green */
        assert_close(v1->argb[0], 0.0f, 0.001f);
        assert_close(v1->argb[1], 1.0f, 0.001f);
        assert_close(v1->argb[2], 0.0f, 0.001f);
        assert_close(v1->argb[3], 1.0f, 0.001f);

        /* v2 = blue */
        assert_close(v2->argb[0], 0.0f, 0.001f);
        assert_close(v2->argb[1], 0.0f, 0.001f);
        assert_close(v2->argb[2], 1.0f, 0.001f);
        assert_close(v2->argb[3], 1.0f, 0.001f);
    }

    /* Same test as above but using GL_UNSIGNED_BYTE colours (RGBA order). */
    void test_color_pointer_ubyte4_rgba_stored_correctly() {
        GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             0.0f,  1.0f, 0.5f
        };
        /* RGBA ubyte colours: red, green, blue */
        GLubyte colors[] = {
            255,   0,   0, 255,   /* red   */
              0, 255,   0, 255,   /* green */
              0,   0, 255, 255    /* blue  */
        };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        Vertex* v0 = vertex_at(&OP_LIST, 1);
        Vertex* v1 = vertex_at(&OP_LIST, 2);
        Vertex* v2 = vertex_at(&OP_LIST, 3);

        /* v0 = red */
        assert_close(v0->argb[R8IDX], 1.0f, 0.005f);
        assert_close(v0->argb[G8IDX], 0.0f, 0.005f);
        assert_close(v0->argb[B8IDX], 0.0f, 0.005f);
        assert_close(v0->argb[A8IDX], 1.0f, 0.005f);

        /* v1 = green */
        assert_close(v1->argb[R8IDX], 0.0f, 0.005f);
        assert_close(v1->argb[G8IDX], 1.0f, 0.005f);
        assert_close(v1->argb[B8IDX], 0.0f, 0.005f);
        assert_close(v1->argb[A8IDX], 1.0f, 0.005f);

        /* v2 = blue */
        assert_close(v2->argb[R8IDX], 0.0f, 0.005f);
        assert_close(v2->argb[G8IDX], 0.0f, 0.005f);
        assert_close(v2->argb[B8IDX], 1.0f, 0.005f);
        assert_close(v2->argb[A8IDX], 1.0f, 0.005f);
    }

    /* Three-component float colour pointer (GL_FLOAT, size 3).
     * _readColour3fARGB reads RGB and forces alpha to 1.0. */
    void test_color_pointer_float3_rgb_forces_alpha_to_one() {
        GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             0.0f,  1.0f, 0.5f
        };
        /* RGB float colours only (no alpha channel) */
        GLfloat colors[] = {
            0.5f, 0.0f, 0.0f,   /* dark red   */
            0.0f, 0.5f, 0.0f,   /* dark green */
            0.0f, 0.0f, 0.5f    /* dark blue  */
        };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glColorPointer(3, GL_FLOAT, 0, colors);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        Vertex* v0 = vertex_at(&OP_LIST, 1);
        /* Alpha must be 1 even though input has no alpha channel. */
        assert_close(v0->argb[A8IDX], 1.0f, 0.001f);
        assert_close(v0->argb[R8IDX], 0.5f, 0.001f);
        assert_close(v0->argb[G8IDX], 0.0f, 0.001f);
        assert_close(v0->argb[B8IDX], 0.0f, 0.001f);

        Vertex* v1 = vertex_at(&OP_LIST, 2);
        assert_close(v1->argb[A8IDX], 1.0f, 0.001f);
        assert_close(v1->argb[R8IDX], 0.0f, 0.001f);
        assert_close(v1->argb[G8IDX], 0.5f, 0.001f);
        assert_close(v1->argb[B8IDX], 0.0f, 0.001f);
    }

    /* Using glDrawElements with an index array must produce the same vertex
     * colours as the equivalent glDrawArrays call. */
    void test_draw_elements_colour_matches_draw_arrays() {
        GLfloat verts[] = {
            -1.0f, -1.0f, 0.5f,   /* 0 */
             1.0f, -1.0f, 0.5f,   /* 1 */
             0.0f,  1.0f, 0.5f    /* 2 */
        };
        GLfloat colors[] = {
            0.2f, 0.4f, 0.6f, 1.0f,   /* 0 */
            0.3f, 0.5f, 0.7f, 1.0f,   /* 1 */
            0.4f, 0.6f, 0.8f, 1.0f    /* 2 */
        };
        GLubyte indices[] = { 0, 1, 2 };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, indices);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        Vertex* v0 = vertex_at(&OP_LIST, 1);
        assert_close(v0->argb[0], 0.2f, 0.001f);
        assert_close(v0->argb[1], 0.4f, 0.001f);
        assert_close(v0->argb[2], 0.6f, 0.001f);
        assert_close(v0->argb[3], 1.0f, 0.001f);
    }

    /* Blending enabled must route geometry to TR_LIST, not OP_LIST. */
    void test_blended_triangle_goes_to_tr_list() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBegin(GL_TRIANGLES);
            glColor4f(1.0f, 0.0f, 0.0f, 0.5f);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        /* Opaque list must remain empty. */
        assert_equal(aligned_vector_size(&OP_LIST.vector), 0u);
        /* Transparent list must have 1 header + 3 vertices. */
        assert_equal(aligned_vector_size(&TR_LIST.vector), 4u);

        glDisable(GL_BLEND);
    }

    /* Colours in the TR_LIST must also reflect what was submitted. */
    void test_blended_triangle_colour_in_tr_list() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(0.0f, 1.0f, 0.0f, 0.5f);  /* semi-transparent green */

        glBegin(GL_TRIANGLES);
            glVertex3f(-1.0f, -1.0f, 0.5f);
            glVertex3f( 1.0f, -1.0f, 0.5f);
            glVertex3f( 0.0f,  1.0f, 0.5f);
        glEnd();

        Vertex* v = (Vertex*) aligned_vector_at(&TR_LIST.vector, 1);
        assert_close(v->argb[R8IDX], 0.0f, 0.001f);
        assert_close(v->argb[G8IDX], 1.0f, 0.001f);
        assert_close(v->argb[B8IDX], 0.0f, 0.001f);
        assert_close(v->argb[A8IDX], 0.5f, 0.001f);

        glDisable(GL_BLEND);
    }
};
