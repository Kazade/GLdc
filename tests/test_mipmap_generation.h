#pragma once

#include "tools/test.h"
#include "tools/gl_test.h"

#include <stdint.h>
#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glkos.h>

#include "GL/private.h"

class MipmapGenerationTests : public GLTestCase {
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

    static const uint16_t* level16(GLuint level) {
        return (const uint16_t*)_glGetMipmapLocation(_glGetBoundTexture(), level);
    }

    void test_rgb565_averages_each_twiddled_2x2_block() {
        std::vector<uint8_t> image(8 * 8 * 3);
        const uint8_t colors[4][3] = {
            {255,   0,   0},
            {  0, 255,   0},
            {  0,   0, 255},
            {255, 255, 255},
        };

        for(int y = 0; y < 8; ++y) {
            for(int x = 0; x < 8; ++x) {
                const int source = ((y & 1) << 1) | (x & 1);
                const int offset = (y * 8 + x) * 3;
                image[offset + 0] = colors[source][0];
                image[offset + 1] = colors[source][1];
                image[offset + 2] = colors[source][2];
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565_TWID_KOS,
                     8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t expected = 0x7BEF;
        const uint16_t* mip = level16(1);
        for(int i = 0; i < 16; ++i)
            assert_equal(mip[i], expected);
    }

    void test_argb4444_averages_nibbles_without_requantizing() {
        std::vector<uint8_t> image(8 * 8 * 4);
        const uint8_t colors[4][4] = {
            {0xF0, 0x10, 0x20, 0xF0},
            {0x10, 0xF0, 0x40, 0x00},
            {0x20, 0x30, 0xF0, 0x80},
            {0x80, 0x40, 0x10, 0x40},
        };

        for(int y = 0; y < 8; ++y) {
            for(int x = 0; x < 8; ++x) {
                const int source = ((y & 1) << 1) | (x & 1);
                const int offset = (y * 8 + x) * 4;
                image[offset + 0] = colors[source][0];
                image[offset + 1] = colors[source][1];
                image[offset + 2] = colors[source][2];
                image[offset + 3] = colors[source][3];
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB4444_TWID_KOS,
                     8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        assert_equal(glGetError(), GL_NO_ERROR);

        const uint16_t* mip = level16(1);
        for(int i = 0; i < 16; ++i)
            assert_equal(mip[i], (uint16_t)0x6655);
    }

    void test_argb1555_preserves_half_covered_alpha_features() {
        for(int opaque_count = 0; opaque_count <= 4; ++opaque_count) {
            uint16_t image[8 * 8];

            if(opaque_count > 0) {
                glDeleteTextures(1, &tex);
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
            }

            for(int y = 0; y < 8; ++y) {
                for(int x = 0; x < 8; ++x) {
                    const int source = ((y & 1) << 1) | (x & 1);
                    image[y * 8 + x] = (uint16_t)(0x03E0 |
                        (source < opaque_count ? 0x8000 : 0x0000));
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB1555_TWID_KOS,
                         8, 8, 0, GL_BGRA,
                         GL_UNSIGNED_SHORT_1_5_5_5_REV, image);
            glGenerateMipmap(GL_TEXTURE_2D);
            assert_equal(glGetError(), GL_NO_ERROR);

            const uint16_t expected = (uint16_t)(0x03E0 |
                (opaque_count >= 2 ? 0x8000 : 0x0000));
            const uint16_t* mip = level16(1);
            for(int i = 0; i < 16; ++i)
                assert_equal(mip[i], expected);
        }
    }
};
