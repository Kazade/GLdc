#include <stddef.h>
#include <time.h>
#include <stdio.h>

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include <GL/gl.h>
#include <GL/glkos.h>

#include "image.h"


int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    glKosInit();
    glClearColor(0.5f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glKosSwapBuffers();

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    time_t start = time(NULL);
    time_t end = start;

    int counter = 0;

    fprintf(stderr, "Starting test run...\n");

    while((end - start) < 5) {
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, header_data
        );

        ++counter;
        end = time(NULL);
    }

    fprintf(stderr, "Called glTexImage2D %d times (%.4f per call)\n", counter, (float)(end - start) / (float)(counter));

    return 0;
}
