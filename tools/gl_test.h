/*
 * Shared test fixture for GLdc tests that drive the GL API.
 *
 * On the Dreamcast the GPU/PVR (and the display) must be initialised exactly
 * once: InitGPU() calls pvr_init() and ShutdownGPU() calls pvr_shutdown(), and
 * tearing those down and back up between every test is both slow and fragile on
 * real hardware / emulators. So this fixture:
 *
 *   - initialises the GPU lazily on the first test and registers an atexit()
 *     handler to shut it down once, at program exit (window opens at start,
 *     closes at the end);
 *   - resets the mutable GL state to a known baseline before each test, without
 *     re-initialising the GPU.
 *
 * GL-driving test suites should derive from GLTestCase (instead of
 * test::TestCase). If a suite needs extra per-test setup it should override
 * set_up()/tear_down() and call GLTestCase::set_up()/tear_down() first.
 */
#pragma once

#include <cstdlib>

#include "tools/test.h"

#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/private.h"
#include "GL/state.h"
#include "containers/aligned_vector.h"

class GLTestCase : public test::TestCase {
public:
    /* One-shot init flag (function-local static to keep this header-only). */
    static bool& gpu_initialized() {
        static bool v = false;
        return v;
    }

    static void shutdown_gpu_at_exit() {
        glKosShutdown();
    }

    static void ensure_gpu_initialized() {
        if(gpu_initialized()) {
            return;
        }

        GLdcConfig config;
        glKosInitConfig(&config);
        config.texture_twiddle = GL_FALSE;
        glKosInitEx(&config);

        gpu_initialized() = true;

        /* Close the window / shut the PVR down once, when the process exits. */
        atexit(GLTestCase::shutdown_gpu_at_exit);
    }

    /* Return all GL state to a predictable baseline. This deliberately avoids
     * the allocating _glInit* helpers (immediate-mode buffer, texture/named
     * arrays, framebuffers) so it is safe to call repeatedly. */
    void reset_gl_state() {
        aligned_vector_clear(&OP_LIST.vector);
        aligned_vector_clear(&PT_LIST.vector);
        aligned_vector_clear(&TR_LIST.vector);

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_TEXTURE_TWIDDLE_KOS);

        glBlendFunc(GL_ONE, GL_ZERO);
        glDepthFunc(GL_LESS);
        glShadeModel(GL_SMOOTH);

        glBindTexture(GL_TEXTURE_2D, 0);

        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glViewport(0, 0, 640, 480);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glNormal3f(0.0f, 0.0f, 1.0f);

        ATTRIB_LIST.enabled = 0;
        ATTRIB_LIST.dirty   = ~0u;

        _glGPUStateMarkDirty();

        /* Drain any error raised while resetting. */
        while(glGetError() != GL_NO_ERROR) {}
    }

    void set_up() {
        ensure_gpu_initialized();
        reset_gl_state();
    }

    void tear_down() {}
};
