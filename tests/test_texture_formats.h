#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <stdint.h>
#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glkos.h>

#include "GL/private.h"

/* =========================================================================
 * TextureFormatTests
 *
 * Regression coverage for texture *loading*: that glTexImage2D / glTexSubImage2D
 * pick the right internal format and, crucially, that the host->PVR pixel
 * conversion produces the exact expected bytes. These checks are byte-exact and
 * fully deterministic, so they pin down the conversion routines (RGB565,
 * ARGB4444, ARGB1555, RGBA8, RED/ALPHA/LUMINANCE sources, paletted) against
 * accidental changes.
 *
 * Twiddling is left OFF for the byte-exact tests so the stored data is a plain
 * row-major array of 16/32-bit texels we can index directly. Twiddled layouts
 * are validated at the internal-format level only.
 * =========================================================================*/
class TextureFormatTests : public GLTestCase {
public:
    GLuint tex = 0;

    void set_up() {
        GLTestCase::set_up();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
    }

    void tear_down() {
        glDeleteTextures(1, &tex);
        GLTestCase::tear_down();
    }

    /* The converted 16bpp texel array for the currently bound texture. */
    static const uint16_t* data16() {
        return (const uint16_t*) _glGetBoundTexture()->data;
    }
    static const uint8_t* data8() {
        return (const uint8_t*) _glGetBoundTexture()->data;
    }
    static GLint internal_format() {
        GLint f;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &f);
        return f;
    }

    /* ------------------------------------------------------ Internal format */

    void test_rgb_ubyte_selects_rgb565() {
        std::vector<uint8_t> img(8 * 8 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);
        assert_equal(internal_format(), GL_RGB565_KOS);
    }

    void test_rgba_ubyte_selects_argb4444() {
        std::vector<uint8_t> img(8 * 8 * 4, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);
        assert_equal(internal_format(), GL_ARGB4444_KOS);
    }

    void test_twiddle_enabled_selects_twiddled_format() {
        std::vector<uint8_t> img(8 * 8 * 3, 0);
        glEnable(GL_TEXTURE_TWIDDLE_KOS);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        glDisable(GL_TEXTURE_TWIDDLE_KOS);
        assert_equal(glGetError(), GL_NO_ERROR);
        assert_equal(internal_format(), GL_RGB565_TWID_KOS);
    }

    /* ----------------------------------------------------- Dimensions/state */

    void test_dimensions_are_stored() {
        std::vector<uint8_t> img(16 * 32 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);
        TextureObject* t = _glGetBoundTexture();
        assert_equal((int) t->width, 16);
        assert_equal((int) t->height, 32);
    }

    /* ------------------------------------------------- Byte-exact: RGB565 */

    void test_rgb888_to_rgb565_packing() {
        /* Four distinct texels with known RGB values. */
        uint8_t img[8 * 8 * 3] = {0};
        uint8_t colors[4][3] = {
            {255,   0,   0},  /* red   */
            {  0, 255,   0},  /* green */
            {  0,   0, 255},  /* blue  */
            {130,  70,  30},  /* arbitrary */
        };
        for(int i = 0; i < 4; ++i) {
            img[i * 3 + 0] = colors[i][0];
            img[i * 3 + 1] = colors[i][1];
            img[i * 3 + 2] = colors[i][2];
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* d = data16();
        for(int i = 0; i < 4; ++i) {
            uint16_t r = (colors[i][0] >> 3) & 0x1f;
            uint16_t g = (colors[i][1] >> 2) & 0x3f;
            uint16_t b = (colors[i][2] >> 3) & 0x1f;
            uint16_t expected = (r << 11) | (g << 5) | b;
            assert_equal(d[i], expected);
        }
    }

    void test_red_ubyte_to_rgb565_only_sets_red() {
        uint8_t img[8 * 8] = {0};
        img[0] = 0xFF;  /* full red */
        img[1] = 0x80;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_KOS, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, img);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* d = data16();
        /* _r8_to_rgb565: (r & 0xF8) << 8  -> only top 5 red bits, green/blue zero */
        assert_equal(d[0], (uint16_t)((0xFF & 0xF8) << 8));
        assert_equal(d[1], (uint16_t)((0x80 & 0xF8) << 8));
        /* green/blue bits must be clear */
        assert_equal((int)(d[0] & 0x07FF), 0);
    }

    /* ------------------------------------------------- Byte-exact: ARGB4444 */

    void test_rgba8888_to_argb4444_packing() {
        uint8_t img[8 * 8 * 4] = {0};
        uint8_t colors[4][4] = {
            {0xFF, 0x00, 0x00, 0xFF},  /* opaque red   */
            {0x00, 0xFF, 0x00, 0x80},  /* half green   */
            {0x00, 0x00, 0xFF, 0x00},  /* transp blue  */
            {0x12, 0x34, 0x56, 0x78},  /* arbitrary    */
        };
        for(int i = 0; i < 4; ++i)
            for(int c = 0; c < 4; ++c)
                img[i * 4 + c] = colors[i][c];

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* d = data16();
        for(int i = 0; i < 4; ++i) {
            uint8_t r = colors[i][0], g = colors[i][1], b = colors[i][2], a = colors[i][3];
            uint16_t expected = ((a & 0xF0) << 8) | ((r & 0xF0) << 4) | (g & 0xF0) | ((b & 0xF0) >> 4);
            assert_equal(d[i], expected);
        }
    }

    void test_alpha_ubyte_to_argb4444_replicates_alpha() {
        uint8_t img[8 * 8] = {0};
        img[0] = 0xFF;
        img[1] = 0x00;
        img[2] = 0x5A;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB4444_KOS, 8, 8, 0, GL_ALPHA, GL_UNSIGNED_BYTE, img);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* d = data16();
        /* _a8_to_argb4444 replicates the top nibble across all 4 channels. */
        for(int i = 0; i < 3; ++i) {
            uint8_t a4 = (img[i] >> 4) & 0xF;
            uint16_t expected = (a4 << 12) | (a4 << 8) | (a4 << 4) | a4;
            assert_equal(d[i], expected);
        }
    }

    /* GL_RGBA8 is not a native PVR texture format in GLdc; it is downgraded to
     * ARGB4444. Pin that behaviour so a change is noticed. */
    void test_rgba8_is_downgraded_to_argb4444() {
        std::vector<uint8_t> img(8 * 8 * 4, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);
        assert_equal(internal_format(), GL_ARGB4444_KOS);
    }

    /* ------------------------------------------------- glTexSubImage2D */

    void test_subimage_updates_only_target_region() {
        /* Start with an all-black 16x16 RGB565 texture. */
        std::vector<uint8_t> img(16 * 16 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);

        /* Replace a 2x2 block at (4,4) with red. */
        uint8_t sub[2 * 2 * 3];
        for(int i = 0; i < 4; ++i) { sub[i*3+0] = 255; sub[i*3+1] = 0; sub[i*3+2] = 0; }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 4, 4, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, sub);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* d = data16();
        uint16_t red565 = (uint16_t)((0xFF >> 3) << 11);

        /* Updated texels are red, a neighbour outside the region is still black. */
        assert_equal(d[4 * 16 + 4], red565);
        assert_equal(d[4 * 16 + 5], red565);
        assert_equal(d[5 * 16 + 4], red565);
        assert_equal(d[5 * 16 + 5], red565);
        assert_equal(d[4 * 16 + 6], (uint16_t) 0);  /* just right of region */
        assert_equal(d[0], (uint16_t) 0);           /* corner untouched */
    }

    /* ------------------------------------------------- Paletted textures */

    void test_color_index8_is_paletted() {
        std::vector<uint8_t> img(8 * 8, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 8, 8, 0,
                     GL_COLOR_INDEX, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_NO_ERROR);

        TextureObject* t = _glGetBoundTexture();
        assert_true(t->isPaletted);
        /* Paletted formats are always twiddled internally. */
        assert_equal(t->internalFormat, GL_COLOR_INDEX8_TWID_KOS);
    }

    void test_color_table_then_indexed_texture_no_error() {
        /* A 4 entry RGBA palette. */
        uint8_t palette[4 * 4] = {
            255,   0,   0, 255,
              0, 255,   0, 255,
              0,   0, 255, 255,
            255, 255, 255, 255,
        };
        glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 4, GL_RGBA, GL_UNSIGNED_BYTE, palette);
        assert_equal(glGetError(), GL_NO_ERROR);

        uint8_t indices[8 * 8];
        for(int i = 0; i < 8 * 8; ++i) indices[i] = i & 3;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 8, 8, 0,
                     GL_COLOR_INDEX, GL_UNSIGNED_BYTE, indices);
        assert_equal(glGetError(), GL_NO_ERROR);

        TextureObject* t = _glGetBoundTexture();
        assert_is_not_null(t->palette);
    }

    /* ------------------------------------------------- Error handling */

    void test_non_power_of_two_width_raises_invalid_value() {
        std::vector<uint8_t> img(24 * 8 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 24, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_oversized_texture_raises_invalid_value() {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2048, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_invalid_target_raises_invalid_enum() {
        std::vector<uint8_t> img(8 * 8 * 3, 0);
        /* Any target other than GL_TEXTURE_2D is unsupported. */
        glTexImage2D((GLenum) 0x0DE0 /* GL_TEXTURE_1D */, 0, GL_RGB, 8, 8, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, img.data());
        assert_equal(glGetError(), GL_INVALID_ENUM);
    }

    /* ------------------------------------------------- Multiple textures */

    void test_two_textures_keep_independent_data() {
        GLuint a = 0, b = 0;
        glGenTextures(1, &a);
        glGenTextures(1, &b);

        uint8_t reds[8 * 8 * 3];
        uint8_t blues[8 * 8 * 3];
        for(int i = 0; i < 8 * 8; ++i) {
            reds[i*3+0] = 255; reds[i*3+1] = 0; reds[i*3+2] = 0;
            blues[i*3+0] = 0;  blues[i*3+1] = 0; blues[i*3+2] = 255;
        }

        glBindTexture(GL_TEXTURE_2D, a);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, reds);
        glBindTexture(GL_TEXTURE_2D, b);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, blues);
        assert_equal(glGetError(), GL_NO_ERROR);

        uint16_t red565  = (uint16_t)((0xFF >> 3) << 11);
        uint16_t blue565 = (uint16_t)((0xFF >> 3));

        glBindTexture(GL_TEXTURE_2D, a);
        assert_equal(data16()[0], red565);
        glBindTexture(GL_TEXTURE_2D, b);
        assert_equal(data16()[0], blue565);

        glDeleteTextures(1, &a);
        glDeleteTextures(1, &b);
    }

    /* Re-uploading a different size to the same texture must reallocate and
     * store the new dimensions. */
    void test_reupload_different_size_updates_dimensions() {
        std::vector<uint8_t> img(8 * 8 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());

        std::vector<uint8_t> img2(32 * 32 * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, img2.data());
        assert_equal(glGetError(), GL_NO_ERROR);

        TextureObject* t = _glGetBoundTexture();
        assert_equal((int) t->width, 32);
        assert_equal((int) t->height, 32);
    }
};
