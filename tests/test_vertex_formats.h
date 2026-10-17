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
 * VertexFormatTests
 *
 * Coverage for the vertex-position attribute readers used by glDrawArrays /
 * glDrawElements (GL/attributes.c). Each supported pointer type (float, short,
 * int, byte, double) and size (2 / 3) is exercised, and the *submitted* vertex
 * positions are compared against an equivalent GL_FLOAT draw.
 *
 * This pins down the per-type interpretation, e.g. that GL_SHORT/GL_INT are
 * read as raw values while GL_UNSIGNED_BYTE is normalised by 1/255, so a
 * regression in any reader is caught without depending on the exact viewport
 * matrix maths.
 * =========================================================================*/
class VertexFormatTests : public GLTestCase {
public:
    void set_up() {
        GLTestCase::set_up();
        glEnableClientState(GL_VERTEX_ARRAY);
    }

    /* Empty OP_LIST and force a fresh poly header on the next draw. */
    static void reset_list() {
        aligned_vector_clear(&OP_LIST.vector);
        _glGPUStateMarkDirty();
    }

    /* All vertex-flagged entries currently in OP_LIST (headers skipped). */
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

    void assert_positions_match(const std::vector<Vertex>& a, const std::vector<Vertex>& b) {
        assert_equal(a.size(), b.size());
        for(size_t i = 0; i < a.size(); ++i) {
            assert_close(a[i].xyz[0], b[i].xyz[0], 0.01f);
            assert_close(a[i].xyz[1], b[i].xyz[1], 0.01f);
            assert_close(a[i].xyz[2], b[i].xyz[2], 0.01f);
            assert_close(a[i].w,      b[i].w,      0.01f);
        }
    }

    /* --------------------------------------------------------- float (ref) */

    void test_float_size3_basic_submission() {
        GLfloat verts[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };
        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        std::vector<Vertex> v = captured();
        assert_equal(v.size(), (size_t) 3);
        /* w must be 1 for an identity transform of a z=0 triangle. */
        assert_close(v[0].w, 1.0f, 0.001f);
    }

    /* --------------------------------------------------------- short == float */

    void test_short_matches_equivalent_float() {
        GLshort  sverts[] = { 2, 3,   5, 7,   11, 1 };       /* size 2 */
        GLfloat  fverts[] = { 2, 3,   5, 7,   11, 1 };

        reset_list();
        glVertexPointer(2, GL_FLOAT, 0, fverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> fref = captured();

        reset_list();
        glVertexPointer(2, GL_SHORT, 0, sverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> sres = captured();

        assert_positions_match(fref, sres);
    }

    /* --------------------------------------------------------- int == float */

    void test_int_matches_equivalent_float() {
        GLint    iverts[] = { 1, 4,   6, 2,   3, 9 };
        GLfloat  fverts[] = { 1, 4,   6, 2,   3, 9 };

        reset_list();
        glVertexPointer(2, GL_FLOAT, 0, fverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> fref = captured();

        reset_list();
        glVertexPointer(2, GL_INT, 0, iverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> ires = captured();

        assert_positions_match(fref, ires);
    }

    /* --------------------------------------------------------- double == float */

    void test_double_matches_equivalent_float() {
        GLdouble dverts[] = { -0.5, -0.5,  0.5, -0.5,  0.0, 0.5 };
        GLfloat  fverts[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };

        reset_list();
        glVertexPointer(2, GL_FLOAT, 0, fverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> fref = captured();

        reset_list();
        glVertexPointer(2, GL_DOUBLE, 0, dverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> dres = captured();

        assert_positions_match(fref, dres);
    }

    /* GL_UNSIGNED_BYTE positions are normalised by 1/255. */
    void test_ubyte_matches_normalized_float() {
        GLubyte  bverts[] = { 10, 20,   30, 40,   50, 60 };
        GLfloat  fverts[] = {
            10.0f/255.0f, 20.0f/255.0f,
            30.0f/255.0f, 40.0f/255.0f,
            50.0f/255.0f, 60.0f/255.0f,
        };

        reset_list();
        glVertexPointer(2, GL_FLOAT, 0, fverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> fref = captured();

        reset_list();
        glVertexPointer(2, GL_UNSIGNED_BYTE, 0, bverts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> bres = captured();

        assert_positions_match(fref, bres);
    }

    /* --------------------------------------------------------- size 2 vs 3 */

    /* A size-2 pointer must produce z == 0 (and therefore match a size-3 draw
     * whose z components are all zero). */
    void test_size2_implies_zero_z() {
        GLfloat v2[] = { -0.5f, -0.5f,  0.5f, -0.5f,  0.0f, 0.5f };
        GLfloat v3[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, v3);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> r3 = captured();

        reset_list();
        glVertexPointer(2, GL_FLOAT, 0, v2);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> r2 = captured();

        assert_positions_match(r3, r2);
    }

    /* --------------------------------------------------------- stride */

    /* An interleaved array with padding between vertices must read the same
     * positions as a tightly-packed one. */
    void test_custom_stride_is_respected() {
        GLfloat tight[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };
        /* Same positions, but each followed by 2 padding floats (stride 20). */
        GLfloat padded[] = {
            -0.5f, -0.5f, 0.0f, 99.0f, 99.0f,
             0.5f, -0.5f, 0.0f, 99.0f, 99.0f,
             0.0f,  0.5f, 0.0f, 99.0f, 99.0f,
        };

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, tight);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> rt = captured();

        reset_list();
        glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), padded);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> rp = captured();

        assert_positions_match(rt, rp);
    }

    /* --------------------------------------------------------- glDrawElements */

    void test_draw_elements_matches_draw_arrays() {
        GLfloat verts[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };
        GLubyte indices[] = { 0, 1, 2 };

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> da = captured();

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, indices);
        std::vector<Vertex> de = captured();

        assert_positions_match(da, de);
    }

    /* glDrawElements honours the index order. */
    void test_draw_elements_reorders_vertices() {
        GLfloat verts[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };
        GLubyte forward[]  = { 0, 1, 2 };
        GLubyte reversed[] = { 2, 1, 0 };

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, forward);
        std::vector<Vertex> f = captured();

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, reversed);
        std::vector<Vertex> r = captured();

        assert_equal(f.size(), (size_t) 3);
        /* First of forward == last of reversed. */
        assert_close(f[0].xyz[0], r[2].xyz[0], 0.01f);
        assert_close(f[0].xyz[1], r[2].xyz[1], 0.01f);
        assert_close(f[2].xyz[0], r[0].xyz[0], 0.01f);
    }

    /* --------------------------------------------------------- immediate mode */

    /* glVertex2f(x,y) must equal glVertex3f(x,y,0). */
    void test_immediate_vertex2f_equals_vertex3f_zero_z() {
        reset_list();
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.5f, -0.5f, 0.0f);
            glVertex3f( 0.5f, -0.5f, 0.0f);
            glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();
        std::vector<Vertex> v3 = captured();

        reset_list();
        glBegin(GL_TRIANGLES);
            glVertex2f(-0.5f, -0.5f);
            glVertex2f( 0.5f, -0.5f);
            glVertex2f( 0.0f,  0.5f);
        glEnd();
        std::vector<Vertex> v2 = captured();

        assert_positions_match(v3, v2);
    }

    /* Immediate mode must produce the same submission as an equivalent
     * vertex-array draw. */
    void test_immediate_matches_vertex_array() {
        GLfloat verts[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> arr = captured();

        reset_list();
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.5f, -0.5f, 0.0f);
            glVertex3f( 0.5f, -0.5f, 0.0f);
            glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();
        std::vector<Vertex> imm = captured();

        assert_positions_match(arr, imm);
    }
};
