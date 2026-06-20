#include "tools/test.h"

#include <stdint.h>
#include <GL/gl.h>
#include <GL/glkos.h>


class TexImage2DTests : public test::TestCase {
public:
    uint8_t image_data[8 * 8 * 4] = {0};
    uint8_t stride_image_data[96 * 48 * 4] = {0};

    void set_up() {
        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = false;
        glKosInitEx(&config);

        /* Init image data so each texel RGBA value matches the
         * position in the array */
        for(int i = 0; i < 8 * 8 * 4; i += 4) {
            image_data[i + 0] = i;
            image_data[i + 1] = i;
            image_data[i + 2] = i;
            image_data[i + 3] = i;
        }
    }

    void tear_down() {
        glKosShutdown();
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

    void test_npot_rejected_without_stride_parameter() {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_strided_npot_upload_allowed_after_opt_in() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 96);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_NO_ERROR);

        GLint internalFormat;
        glGetIntegerv(GL_TEXTURE_INTERNAL_FORMAT_KOS, &internalFormat);

        assert_equal(internalFormat, GL_RGB565_KOS);
    }

    void test_strided_texture_width_must_match_stride() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 64);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_strided_texture_rejects_invalid_stride_parameter() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 65);
        assert_equal(glGetError(), GL_INVALID_VALUE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 1024);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }

    void test_strided_texture_rejects_twiddled_storage() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 96);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB_TWID_KOS, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_OPERATION);
    }

    void test_strided_texture_rejects_mipmap_level() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STRIDE_KOS, 96);
        assert_equal(glGetError(), GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 96, 48, 0, GL_RGB, GL_UNSIGNED_BYTE, stride_image_data);
        assert_equal(glGetError(), GL_INVALID_VALUE);
    }
};
