#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <stdint.h>
#include <climits>
#include <vector>
#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/private.h"
#include "GL/state.h"
#include "containers/aligned_vector.h"

/* =========================================================================
 * TexCoordFormatTests
 *
 * Coverage for the texture-coordinate attribute readers (GL/attributes.c).
 * The supported glTexCoordPointer types each normalise differently:
 *   GL_FLOAT / GL_DOUBLE  - passed through
 *   GL_UNSIGNED_BYTE      - divided by 255
 *   GL_SHORT              - divided by SHRT_MAX (32767)
 *   GL_INT                - passed through (raw)
 * Each is compared against an equivalent GL_FLOAT draw so the per-type scaling
 * is pinned down. Immediate-mode glTexCoord2f and the disabled-array (zeroed)
 * case are covered too.
 * =========================================================================*/
class TexCoordFormatTests : public GLTestCase {
public:
    void set_up() {
        GLTestCase::set_up();
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    static void reset_list() {
        aligned_vector_clear(&OP_LIST.vector);
        _glGPUStateMarkDirty();
    }

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

    void assert_uvs_match(const std::vector<Vertex>& a, const std::vector<Vertex>& b) {
        assert_equal(a.size(), b.size());
        for(size_t i = 0; i < a.size(); ++i) {
            assert_close(a[i].uv[0], b[i].uv[0], 0.0005f);
            assert_close(a[i].uv[1], b[i].uv[1], 0.0005f);
        }
    }

    /* A fixed triangle for the position data. */
    static const GLfloat* positions() {
        static const GLfloat verts[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f,
        };
        return verts;
    }

    std::vector<Vertex> draw_with_float_uv(const GLfloat* uv) {
        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_FLOAT, 0, uv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        return captured();
    }

    /* --------------------------------------------------------- float basic */

    void test_float_uv_passthrough() {
        GLfloat uv[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f };
        std::vector<Vertex> v = draw_with_float_uv(uv);
        assert_equal(v.size(), (size_t) 3);
        assert_close(v[0].uv[0], 0.0f, 0.0005f);
        assert_close(v[1].uv[0], 1.0f, 0.0005f);
        assert_close(v[2].uv[1], 1.0f, 0.0005f);
    }

    /* --------------------------------------------------------- short / 32767 */

    void test_short_uv_is_scaled_by_shrt_max() {
        GLshort suv[] = {
            0,            0,
            SHRT_MAX,     0,
            SHRT_MAX / 2, SHRT_MAX,
        };
        GLfloat fuv[] = {
            0.0f,                          0.0f,
            (float) SHRT_MAX / SHRT_MAX,   0.0f,
            (float)(SHRT_MAX / 2) / SHRT_MAX, (float) SHRT_MAX / SHRT_MAX,
        };

        std::vector<Vertex> fref = draw_with_float_uv(fuv);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_SHORT, 0, suv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> sres = captured();

        assert_uvs_match(fref, sres);
    }

    /* --------------------------------------------------------- ubyte / 255 */

    void test_ubyte_uv_is_scaled_by_255() {
        GLubyte buv[] = { 0, 0,   255, 0,   128, 255 };
        GLfloat fuv[] = {
            0.0f,          0.0f,
            255.0f/255.0f, 0.0f,
            128.0f/255.0f, 255.0f/255.0f,
        };

        std::vector<Vertex> fref = draw_with_float_uv(fuv);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_UNSIGNED_BYTE, 0, buv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> bres = captured();

        assert_uvs_match(fref, bres);
    }

    /* --------------------------------------------------------- int raw */

    void test_int_uv_is_raw() {
        GLint iuv[] = { 0, 0,   1, 0,   0, 1 };
        GLfloat fuv[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f };

        std::vector<Vertex> fref = draw_with_float_uv(fuv);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_INT, 0, iuv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> ires = captured();

        assert_uvs_match(fref, ires);
    }

    /* --------------------------------------------------------- double */

    void test_double_uv_passthrough() {
        GLdouble duv[] = { 0.0, 0.0,  1.0, 0.0,  0.5, 1.0 };
        GLfloat  fuv[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f };

        std::vector<Vertex> fref = draw_with_float_uv(fuv);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_DOUBLE, 0, duv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> dres = captured();

        assert_uvs_match(fref, dres);
    }

    /* --------------------------------------------------------- stride */

    void test_uv_stride_is_respected() {
        GLfloat tight[]  = { 0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f };
        GLfloat padded[] = {
            0.0f, 0.0f, 77.0f,
            1.0f, 0.0f, 77.0f,
            0.5f, 1.0f, 77.0f,
        };

        std::vector<Vertex> rt = draw_with_float_uv(tight);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glTexCoordPointer(2, GL_FLOAT, 3 * sizeof(GLfloat), padded);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> rp = captured();

        assert_uvs_match(rt, rp);
    }

    /* --------------------------------------------------------- disabled */

    /* With the texture-coordinate array disabled, every vertex takes the
     * current texture coordinate (set via glTexCoord), matching fixed-function
     * GL semantics. */
    void test_disabled_coord_array_uses_current_texcoord() {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoord2f(0.25f, 0.75f);

        reset_list();
        glVertexPointer(3, GL_FLOAT, 0, positions());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<Vertex> v = captured();

        assert_equal(v.size(), (size_t) 3);
        for(size_t i = 0; i < v.size(); ++i) {
            assert_close(v[i].uv[0], 0.25f, 0.0005f);
            assert_close(v[i].uv[1], 0.75f, 0.0005f);
        }
    }

    /* --------------------------------------------------------- immediate */

    void test_immediate_texcoord2f_reaches_vertex() {
        reset_list();
        glBegin(GL_TRIANGLES);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.5f, -0.5f, 0.0f);
            glTexCoord2f(0.5f, 1.0f); glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();
        std::vector<Vertex> v = captured();

        assert_equal(v.size(), (size_t) 3);
        assert_close(v[0].uv[0], 0.0f, 0.0005f);
        assert_close(v[0].uv[1], 0.0f, 0.0005f);
        assert_close(v[1].uv[0], 1.0f, 0.0005f);
        assert_close(v[2].uv[0], 0.5f, 0.0005f);
        assert_close(v[2].uv[1], 1.0f, 0.0005f);
    }

    /* Immediate texcoords must match the equivalent vertex-array draw. */
    void test_immediate_texcoord_matches_array() {
        GLfloat uv[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 1.0f };
        std::vector<Vertex> arr = draw_with_float_uv(uv);

        reset_list();
        glBegin(GL_TRIANGLES);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.5f, -0.5f, 0.0f);
            glTexCoord2f(0.5f, 1.0f); glVertex3f( 0.0f,  0.5f, 0.0f);
        glEnd();
        std::vector<Vertex> imm = captured();

        assert_uvs_match(arr, imm);
    }
};
