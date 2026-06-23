#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <cstring>
#include <stdint.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glkos.h>


class TexImage2DTests : public GLTestCase {
public:
    uint8_t image_data[8 * 8 * 4] = {0};
    uint8_t stride_image_data[96 * 48 * 4] = {0};
    uint8_t unpack_row_image_data[128 * 48 * 4] = {0};

    void set_up() {
        GLTestCase::set_up();

        /* Init image data so each texel RGBA value matches the
         * position in the array */
        for(int i = 0; i < 8 * 8 * 4; i += 4) {
            image_data[i + 0] = i;
            image_data[i + 1] = i;
            image_data[i + 2] = i;
            image_data[i + 3] = i;
        }
    }

    void test_rgb_to_rgb565() {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_RGB565_KOS);
    }

    void test_rgb_to_rgb565_twiddle() {
        glEnable(GL_TEXTURE_TWIDDLE_KOS);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
        glDisable(GL_TEXTURE_TWIDDLE_KOS);

        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_RGB565_TWID_KOS);
    }

    void test_rgba_to_argb4444() {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_ARGB4444_KOS);
    }

    void test_rgba_to_argb4444_twiddle() {
        glEnable(GL_TEXTURE_TWIDDLE_KOS);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        glDisable(GL_TEXTURE_TWIDDLE_KOS);

        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_ARGB4444_TWID_KOS);
    }

    void test_extension_string_advertises_limited_kos_npot() {
        const char* extensions = (const char*) glGetString(GL_EXTENSIONS);

        assert_true(std::strstr(extensions, "GL_KOS_texture_non_power_of_two") != nullptr);
        assert_true(std::strstr(extensions, "GL_KOS_texture_stride") == nullptr);
        assert_true(std::strstr(extensions, "GL_ARB_texture_non_power_of_two") == nullptr);
    }

    void test_npot_rejected_with_default_repeat_wrap() {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_OPERATION);
    }

    void test_npot_upload_allowed_with_clamp_wrap() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_RGB565_KOS);
    }

    void test_npot_upload_honors_unpack_row_length_without_backend_opt_in() {
        set_clamp_wrap();

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 128);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, unpack_row_image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        assert_equal(glGetError(), GL_NO_ERROR);
    }

    void test_npot_texture_width_must_be_multiple_of_32() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 65, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_npot_texture_width_must_not_exceed_stride_limit() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_npot_texture_rejects_repeat_after_upload() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        assert_equal(glGetError(), GL_INVALID_OPERATION);
    }

    void test_pot_texture_repeat_wrap_unchanged() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        assert_equal(glGetError(), GL_NO_ERROR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
        assert_equal(glGetError(), GL_NO_ERROR);
    }

    void test_npot_texture_rejects_twiddled_storage() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB_TWID_KOS, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_OPERATION);
    }

    void test_npot_texture_rejects_paletted_storage() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 96, 48, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_OPERATION);
    }

    void test_npot_texture_rejects_mipmap_level() {
        set_clamp_wrap();

        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }
};
