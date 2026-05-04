#pragma once

#include "tools/test.h"

#include <stdint.h>
#include <GL/gl.h>
#include <GL/glkos.h>

/* Internal headers for direct state inspection */
#include "GL/state.h"
#include "GL/private.h"

/* =========================================================================
 * GlColorTests
 *
 * Tests for glColor4f / glColor3f / glColor4ub / glColor3ub and their
 * vector variants.  Each call stores a normalised RGBA colour into the
 * internal current-colour array in ARGB component order:
 *   index 0 = alpha  (A8IDX)
 *   index 1 = red    (R8IDX)
 *   index 2 = green  (G8IDX)
 *   index 3 = blue   (B8IDX)
 * =========================================================================*/
class GlColorTests : public test::TestCase {
public:
    void set_up() {
        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = false;
        glKosInitEx(&config);
        /* _glInitContext does not reset current_color, so do it explicitly
         * here so every test starts from a known white default. */
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void tear_down() {
        glKosShutdown();
    }

    /* After set_up the current colour must be white (1,1,1,1). */
    void test_default_color_is_white() {
        float* color = _glCurrentColor();
        assert_close(color[A8IDX], 1.0f, 0.001f);
        assert_close(color[R8IDX], 1.0f, 0.001f);
        assert_close(color[G8IDX], 1.0f, 0.001f);
        assert_close(color[B8IDX], 1.0f, 0.001f);
    }

    /* glColor4f must store each component at its correct ARGB slot. */
    void test_color4f_sets_rgba() {
        glColor4f(0.25f, 0.5f, 0.75f, 0.9f);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.25f, 0.001f);
        assert_close(color[G8IDX], 0.50f, 0.001f);
        assert_close(color[B8IDX], 0.75f, 0.001f);
        assert_close(color[A8IDX], 0.90f, 0.001f);
    }

    /* Sanity check for a pure-red colour. */
    void test_color4f_pure_red() {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 1.0f, 0.001f);
        assert_close(color[G8IDX], 0.0f, 0.001f);
        assert_close(color[B8IDX], 0.0f, 0.001f);
        assert_close(color[A8IDX], 1.0f, 0.001f);
    }

    /* Sanity check for a pure-blue colour. */
    void test_color4f_pure_blue() {
        glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.0f, 0.001f);
        assert_close(color[G8IDX], 0.0f, 0.001f);
        assert_close(color[B8IDX], 1.0f, 0.001f);
        assert_close(color[A8IDX], 1.0f, 0.001f);
    }

    /* The second call must completely overwrite the first. */
    void test_color4f_overwrites_previous_color() {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        glColor4f(0.0f, 1.0f, 0.0f, 0.5f);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.0f, 0.001f);
        assert_close(color[G8IDX], 1.0f, 0.001f);
        assert_close(color[B8IDX], 0.0f, 0.001f);
        assert_close(color[A8IDX], 0.5f, 0.001f);
    }

    /* glColor3f sets RGB and forces alpha to 1.0. */
    void test_color3f_sets_rgb_and_forces_alpha_to_one() {
        glColor3f(0.25f, 0.5f, 0.75f);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.25f, 0.001f);
        assert_close(color[G8IDX], 0.50f, 0.001f);
        assert_close(color[B8IDX], 0.75f, 0.001f);
        assert_close(color[A8IDX], 1.00f, 0.001f);
    }

    /* After a prior glColor4f with alpha=0, glColor3f must reset alpha to 1. */
    void test_color3f_resets_alpha_to_one() {
        glColor4f(0.5f, 0.5f, 0.5f, 0.0f);
        glColor3f(1.0f, 0.0f, 0.0f);
        float* color = _glCurrentColor();
        assert_close(color[A8IDX], 1.0f, 0.001f);
    }

    /* glColor4ub must convert each byte to a normalised float. */
    void test_color4ub_converts_bytes_to_normalized_floats() {
        glColor4ub(255, 128, 64, 192);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 255.0f / 255.0f, 0.005f);
        assert_close(color[G8IDX], 128.0f / 255.0f, 0.005f);
        assert_close(color[B8IDX],  64.0f / 255.0f, 0.005f);
        assert_close(color[A8IDX], 192.0f / 255.0f, 0.005f);
    }

    /* 255 must map to exactly 1.0. */
    void test_color4ub_max_values_map_to_one() {
        glColor4ub(255, 255, 255, 255);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 1.0f, 0.001f);
        assert_close(color[G8IDX], 1.0f, 0.001f);
        assert_close(color[B8IDX], 1.0f, 0.001f);
        assert_close(color[A8IDX], 1.0f, 0.001f);
    }

    /* 0 must map to exactly 0.0. */
    void test_color4ub_zero_values_map_to_zero() {
        glColor4ub(0, 0, 0, 0);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.0f, 0.001f);
        assert_close(color[G8IDX], 0.0f, 0.001f);
        assert_close(color[B8IDX], 0.0f, 0.001f);
        assert_close(color[A8IDX], 0.0f, 0.001f);
    }

    /* glColor3ub sets RGB and forces alpha to 1.0. */
    void test_color3ub_sets_rgb_and_forces_alpha_to_one() {
        glColor3ub(255, 128, 64);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 255.0f / 255.0f, 0.005f);
        assert_close(color[G8IDX], 128.0f / 255.0f, 0.005f);
        assert_close(color[B8IDX],  64.0f / 255.0f, 0.005f);
        assert_close(color[A8IDX],  1.0f,            0.001f);
    }

    /* glColor4fv reads RGBA from array. */
    void test_color4fv_sets_from_array() {
        GLfloat c[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
        glColor4fv(c);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.1f, 0.001f);
        assert_close(color[G8IDX], 0.2f, 0.001f);
        assert_close(color[B8IDX], 0.3f, 0.001f);
        assert_close(color[A8IDX], 0.4f, 0.001f);
    }

    /* glColor3fv reads RGB from array and forces alpha to 1. */
    void test_color3fv_sets_from_array_and_forces_alpha_to_one() {
        GLfloat c[3] = { 0.5f, 0.6f, 0.7f };
        glColor3fv(c);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 0.5f, 0.001f);
        assert_close(color[G8IDX], 0.6f, 0.001f);
        assert_close(color[B8IDX], 0.7f, 0.001f);
        assert_close(color[A8IDX], 1.0f, 0.001f);
    }

    /* glColor4ubv reads RGBA bytes from array. */
    void test_color4ubv_sets_from_array() {
        GLubyte c[4] = { 200, 100, 50, 150 };
        glColor4ubv(c);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX], 200.0f / 255.0f, 0.005f);
        assert_close(color[G8IDX], 100.0f / 255.0f, 0.005f);
        assert_close(color[B8IDX],  50.0f / 255.0f, 0.005f);
        assert_close(color[A8IDX], 150.0f / 255.0f, 0.005f);
    }

    /* glColor3ubv reads RGB bytes and forces alpha to 1. */
    void test_color3ubv_sets_from_array_and_forces_alpha_to_one() {
        GLubyte c[3] = { 255, 0, 128 };
        glColor3ubv(c);
        float* color = _glCurrentColor();
        assert_close(color[R8IDX],   1.0f,            0.001f);
        assert_close(color[G8IDX],   0.0f,            0.001f);
        assert_close(color[B8IDX], 128.0f / 255.0f,  0.005f);
        assert_close(color[A8IDX],   1.0f,            0.001f);
    }

    /* Valid colour calls must not produce a GL error. */
    void test_color_calls_produce_no_gl_error() {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        glColor3f(0.5f, 0.5f, 0.5f);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        glColor4ub(128, 128, 128, 255);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        glColor3ub(200, 100, 50);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
    }
};


/* =========================================================================
 * GlColorPointerTests
 *
 * Tests for glColorPointer.  Verifies that the AttribPointer for colour
 * (ATTRIB_LIST.colour) stores the correct size, type, stride and data
 * pointer, that stride=0 triggers the automatic stride calculation, and
 * that invalid size arguments raise GL_INVALID_VALUE.
 * =========================================================================*/
class GlColorPointerTests : public test::TestCase {
public:
    void set_up() {
        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = false;
        glKosInitEx(&config);
        /* Reset enabled flags – _glInitAttributePointers does not do this. */
        ATTRIB_LIST.enabled = 0;
    }

    void tear_down() {
        glKosShutdown();
    }

    /* size=4, GL_FLOAT: stride auto-calculated to 4*4=16. */
    void test_colorpointer_size4_float_stores_metadata() {
        GLfloat colors[12] = {};
        glColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size,   4);
        assert_equal(ATTRIB_LIST.colour.type,   (GLenum)GL_FLOAT);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(4 * sizeof(GLfloat)));
        assert_equal(ATTRIB_LIST.colour.ptr,    (const void*)colors);
    }

    /* size=3, GL_FLOAT: stride auto-calculated to 3*4=12. */
    void test_colorpointer_size3_float_stores_metadata() {
        GLfloat colors[9] = {};
        glColorPointer(3, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size,   3);
        assert_equal(ATTRIB_LIST.colour.type,   (GLenum)GL_FLOAT);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(3 * sizeof(GLfloat)));
    }

    /* size=4, GL_UNSIGNED_BYTE: stride auto-calculated to 4*1=4. */
    void test_colorpointer_size4_ubyte_stores_metadata() {
        GLubyte colors[12] = {};
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size,   4);
        assert_equal(ATTRIB_LIST.colour.type,   (GLenum)GL_UNSIGNED_BYTE);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(4 * sizeof(GLubyte)));
    }

    /* size=3, GL_UNSIGNED_BYTE: stride auto-calculated to 3*1=3. */
    void test_colorpointer_size3_ubyte_stores_metadata() {
        GLubyte colors[9] = {};
        glColorPointer(3, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size,   3);
        assert_equal(ATTRIB_LIST.colour.type,   (GLenum)GL_UNSIGNED_BYTE);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(3 * sizeof(GLubyte)));
    }

    /* GL_BGRA is a valid size token; stride becomes 4 bytes for UNSIGNED_BYTE. */
    void test_colorpointer_bgra_ubyte_is_valid() {
        GLubyte colors[12] = {};
        glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.colour.size,   (GLint)GL_BGRA);
        assert_equal(ATTRIB_LIST.colour.type,   (GLenum)GL_UNSIGNED_BYTE);
        /* 4 bytes per colour element */
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)4);
    }

    /* size=1 is not a legal value; GL_INVALID_VALUE must be raised. */
    void test_colorpointer_size1_raises_invalid_value() {
        GLfloat colors[4] = {};
        glColorPointer(1, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_INVALID_VALUE);
    }

    /* size=2 is not a legal value; GL_INVALID_VALUE must be raised. */
    void test_colorpointer_size2_raises_invalid_value() {
        GLfloat colors[8] = {};
        glColorPointer(2, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_INVALID_VALUE);
    }

    /* Automatic stride for float4 = 16 bytes. */
    void test_colorpointer_auto_stride_float4() {
        GLfloat colors[12] = {};
        glColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(4 * sizeof(GLfloat)));
    }

    /* Automatic stride for float3 = 12 bytes. */
    void test_colorpointer_auto_stride_float3() {
        GLfloat colors[9] = {};
        glColorPointer(3, GL_FLOAT, 0, colors);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(3 * sizeof(GLfloat)));
    }

    /* Automatic stride for ubyte4 = 4 bytes. */
    void test_colorpointer_auto_stride_ubyte4() {
        GLubyte colors[12] = {};
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)(4 * sizeof(GLubyte)));
    }

    /* An explicit non-zero stride must be kept unchanged. */
    void test_colorpointer_explicit_stride_is_preserved() {
        GLfloat colors[12] = {};
        glColorPointer(4, GL_FLOAT, 32, colors);
        assert_equal(ATTRIB_LIST.colour.stride, (GLsizei)32);
    }

    /* The data pointer must be stored verbatim. */
    void test_colorpointer_stores_pointer() {
        GLfloat colors[12] = {};
        glColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(ATTRIB_LIST.colour.ptr, (const void*)colors);
    }

    /* A NULL pointer is accepted without raising an error. */
    void test_colorpointer_null_pointer_is_accepted() {
        glColorPointer(4, GL_FLOAT, 0, NULL);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
    }

    /* Calling glColorPointer must mark the colour attribute as dirty.
     * Uses size=3 to guarantee a state change from the size=4 default
     * initialised by _glInitAttributePointers, avoiding the
     * _glStateUnchanged early-return path. */
    void test_colorpointer_marks_colour_attribute_dirty() {
        GLfloat colors[9] = {};
        ATTRIB_LIST.dirty = 0;
        glColorPointer(3, GL_FLOAT, 0, colors);
        assert_true(ATTRIB_LIST.dirty & COLOR_ENABLED_FLAG);
    }
};


/* =========================================================================
 * GlSecondaryColorPointerTests
 *
 * Mirrors GlColorPointerTests for the secondary (offset) colour pointer
 * stored in ATTRIB_LIST.s_color.
 * =========================================================================*/
class GlSecondaryColorPointerTests : public test::TestCase {
public:
    void set_up() {
        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = false;
        glKosInitEx(&config);
        ATTRIB_LIST.enabled = 0;
    }

    void tear_down() {
        glKosShutdown();
    }

    /* size=4, GL_FLOAT: basic happy path. */
    void test_secondary_colorpointer_size4_float_stores_metadata() {
        GLfloat colors[12] = {};
        glSecondaryColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.s_color.size,   4);
        assert_equal(ATTRIB_LIST.s_color.type,   (GLenum)GL_FLOAT);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)(4 * sizeof(GLfloat)));
        assert_equal(ATTRIB_LIST.s_color.ptr,    (const void*)colors);
    }

    /* size=3, GL_FLOAT: three-component secondary colour. */
    void test_secondary_colorpointer_size3_float_stores_metadata() {
        GLfloat colors[9] = {};
        glSecondaryColorPointer(3, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.s_color.size,   3);
        assert_equal(ATTRIB_LIST.s_color.type,   (GLenum)GL_FLOAT);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)(3 * sizeof(GLfloat)));
    }

    /* GL_BGRA is a valid size for secondary colour. */
    void test_secondary_colorpointer_bgra_ubyte_is_valid() {
        GLubyte colors[12] = {};
        glSecondaryColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_NO_ERROR);
        assert_equal(ATTRIB_LIST.s_color.size,   (GLint)GL_BGRA);
        assert_equal(ATTRIB_LIST.s_color.type,   (GLenum)GL_UNSIGNED_BYTE);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)4);
    }

    /* size=1 must raise GL_INVALID_VALUE. */
    void test_secondary_colorpointer_size1_raises_invalid_value() {
        GLfloat colors[4] = {};
        glSecondaryColorPointer(1, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_INVALID_VALUE);
    }

    /* size=2 must raise GL_INVALID_VALUE. */
    void test_secondary_colorpointer_size2_raises_invalid_value() {
        GLfloat colors[8] = {};
        glSecondaryColorPointer(2, GL_FLOAT, 0, colors);
        assert_equal(glGetError(), (GLenum)GL_INVALID_VALUE);
    }

    /* Automatic stride for float4. */
    void test_secondary_colorpointer_auto_stride_float4() {
        GLfloat colors[12] = {};
        glSecondaryColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)(4 * sizeof(GLfloat)));
    }

    /* Automatic stride for ubyte3. */
    void test_secondary_colorpointer_auto_stride_ubyte3() {
        GLubyte colors[9] = {};
        glSecondaryColorPointer(3, GL_UNSIGNED_BYTE, 0, colors);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)(3 * sizeof(GLubyte)));
    }

    /* An explicit stride must be preserved. */
    void test_secondary_colorpointer_explicit_stride_is_preserved() {
        GLfloat colors[12] = {};
        glSecondaryColorPointer(4, GL_FLOAT, 24, colors);
        assert_equal(ATTRIB_LIST.s_color.stride, (GLsizei)24);
    }

    /* Data pointer must be stored verbatim. */
    void test_secondary_colorpointer_stores_pointer() {
        GLfloat colors[12] = {};
        glSecondaryColorPointer(4, GL_FLOAT, 0, colors);
        assert_equal(ATTRIB_LIST.s_color.ptr, (const void*)colors);
    }

    /* Calling glSecondaryColorPointer must dirty the s_color attribute.
     * _glInitAttributePointers does not touch s_color, so its state
     * persists across tests.  Zeroing it here guarantees a state
     * change so _glStateUnchanged does not short-circuit the call. */
    void test_secondary_colorpointer_marks_attribute_dirty() {
        memset(&ATTRIB_LIST.s_color, 0, sizeof(ATTRIB_LIST.s_color));
        GLfloat colors[12] = {};
        ATTRIB_LIST.dirty = 0;
        glSecondaryColorPointer(4, GL_FLOAT, 0, colors);
        assert_true(ATTRIB_LIST.dirty & S_COLOR_ENABLED_FLAG);
    }
};


/* =========================================================================
 * GlClientStateTests
 *
 * Tests for glEnableClientState / glDisableClientState.  Verifies that the
 * appropriate flag bits in ATTRIB_LIST.enabled are set or cleared, that the
 * dirty flag is updated, and that invalid enumerants raise GL_INVALID_ENUM.
 * =========================================================================*/
class GlClientStateTests : public test::TestCase {
public:
    void set_up() {
        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = false;
        glKosInitEx(&config);
        /* Clear all enabled flags for a clean baseline. */
        ATTRIB_LIST.enabled = 0;
    }

    void tear_down() {
        glKosShutdown();
    }

    /* Enabling GL_COLOR_ARRAY must set COLOR_ENABLED_FLAG. */
    void test_enable_color_array_sets_flag() {
        glEnableClientState(GL_COLOR_ARRAY);
        assert_true(ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG);
    }

    /* Disabling GL_COLOR_ARRAY must clear COLOR_ENABLED_FLAG. */
    void test_disable_color_array_clears_flag() {
        glEnableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        assert_false(ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG);
    }

    /* Enabling GL_VERTEX_ARRAY must set VERTEX_ENABLED_FLAG. */
    void test_enable_vertex_array_sets_flag() {
        glEnableClientState(GL_VERTEX_ARRAY);
        assert_true(ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG);
    }

    /* Disabling GL_VERTEX_ARRAY must clear VERTEX_ENABLED_FLAG. */
    void test_disable_vertex_array_clears_flag() {
        glEnableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        assert_false(ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG);
    }

    /* Enabling GL_NORMAL_ARRAY must set NORMAL_ENABLED_FLAG. */
    void test_enable_normal_array_sets_flag() {
        glEnableClientState(GL_NORMAL_ARRAY);
        assert_true(ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG);
    }

    /* Disabling GL_NORMAL_ARRAY must clear NORMAL_ENABLED_FLAG. */
    void test_disable_normal_array_clears_flag() {
        glEnableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        assert_false(ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG);
    }

    /* With client texture unit 0, GL_TEXTURE_COORD_ARRAY sets UV_ENABLED_FLAG. */
    void test_enable_texture_coord_array_sets_uv_flag() {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        assert_true(ATTRIB_LIST.enabled & UV_ENABLED_FLAG);
    }

    /* Disabling GL_TEXTURE_COORD_ARRAY clears UV_ENABLED_FLAG. */
    void test_disable_texture_coord_array_clears_uv_flag() {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        assert_false(ATTRIB_LIST.enabled & UV_ENABLED_FLAG);
    }

    /* Multiple arrays can be enabled independently. */
    void test_multiple_arrays_enabled_simultaneously() {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        assert_true(ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG);
        assert_true(ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG);
        assert_true(ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG);
    }

    /* Disabling one array must not affect others. */
    void test_disable_one_array_does_not_affect_others() {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        assert_true (ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG);
        assert_false(ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG);
    }

    /* An invalid cap must raise GL_INVALID_ENUM. */
    void test_enable_invalid_cap_raises_invalid_enum() {
        glEnableClientState(0xDEAD);
        assert_equal(glGetError(), (GLenum)GL_INVALID_ENUM);
    }

    /* An invalid cap on disable must raise GL_INVALID_ENUM. */
    void test_disable_invalid_cap_raises_invalid_enum() {
        glDisableClientState(0xDEAD);
        assert_equal(glGetError(), (GLenum)GL_INVALID_ENUM);
    }

    /* Enabling an array must mark the corresponding dirty bit. */
    void test_enable_color_array_marks_dirty() {
        ATTRIB_LIST.dirty = 0;
        glEnableClientState(GL_COLOR_ARRAY);
        assert_true(ATTRIB_LIST.dirty & COLOR_ENABLED_FLAG);
    }

    /* Disabling an array must also mark the dirty bit so that the function
     * pointer is recomputed on the next draw. */
    void test_disable_color_array_marks_dirty() {
        glEnableClientState(GL_COLOR_ARRAY);
        ATTRIB_LIST.dirty = 0;
        glDisableClientState(GL_COLOR_ARRAY);
        assert_true(ATTRIB_LIST.dirty & COLOR_ENABLED_FLAG);
    }
};
