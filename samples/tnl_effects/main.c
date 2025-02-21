#include <GL/gl.h>
#include <math.h>
#include <stdlib.h>

#ifndef _arch_dreamcast
#include <SDL2/SDL.h>
static SDL_Window* win_handle;
#else
#include <kos.h>
#include <GL/glkos.h>
#endif

static void DrawQuad(float x, float y) {
	glBegin(GL_QUADS);
    x -= 1.0f;
    y -= 1.0f;
	glColor3f(0.5f, 0.5f, 0.5f); glTexCoord2f(0.0f, 0.0f); glVertex2f(x + 0.0f, y + 0.0f);
	glColor3f(0.5f, 0.5f, 0.5f); glTexCoord2f(1.0f, 0.0f); glVertex2f(x + 0.3f, y + 0.0f);
	glColor3f(0.5f, 0.5f, 0.5f); glTexCoord2f(1.0f, 1.0f); glVertex2f(x + 0.3f, y + 0.3f);
	glColor3f(0.5f, 0.5f, 0.5f); glTexCoord2f(0.0f, 1.0f); glVertex2f(x + 0.0f, y + 0.3f);
	glEnd();
}

static void sample_init() {
#ifdef SDL2_BUILD
	SDL_Init(SDL_INIT_EVERYTHING);
	win_handle = SDL_CreateWindow("Shapes", 0, 0, 640, 480, SDL_WINDOW_OPENGL);
	SDL_GL_CreateContext(win_handle);
#else
    glKosInit();
#endif
}

static int sample_should_exit() {
#ifndef _arch_dreamcast
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) return 1;
    }
    return 0;
#else
    maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!cont) return 0;

    cont_state_t *state = (cont_state_t *)maple_dev_status(cont);
    if (!state) return 0;

    return state->buttons & CONT_START;
#endif
}

#define IMG_SIZE 8
int main(int argc, char *argv[]) {
    sample_init();
	glClearColor(0.2f, 0.2f, 0.2f, 1);
	glViewport(0, 0, 640, 480);    
    glEnable(GL_TEXTURE_2D);

    GLint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    static GLubyte texData[IMG_SIZE * IMG_SIZE * 4];
	for (int y = 0, i = 0; y < IMG_SIZE; y++)
		for (int x = 0; x < IMG_SIZE; x++, i += 4)
	{
		int a = x + y;
		texData[i + 0] = (a & 1) ? 0xFF : 0;
		texData[i + 1] = (a & 2) ? 0xFF : 0;
		texData[i + 2] = (a & 4) ? 0xFF : 0;
		texData[i + 3] = 0xFF;
	}

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMG_SIZE, IMG_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    float time = 0.0f;

	while (!sample_should_exit()) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		DrawQuad(0.1f, 0.1f);

        glMatrixMode(GL_TEXTURE);
        glTranslatef(time, 0, 0);
        DrawQuad(0.5f, 0.1f);
        glLoadIdentity();

        glMatrixMode(GL_TEXTURE);
        glTranslatef(0, time, 0);
        DrawQuad(0.9f, 0.1f);
        glLoadIdentity();

        glMatrixMode(GL_TEXTURE);
        glScalef(time, time * 0.5f, 0);
        DrawQuad(1.3f, 0.1f);
        glLoadIdentity();

        glMatrixMode(GL_TEXTURE);
        glRotatef(time * 1000, 1, 0, 0);
        DrawQuad(1.7f, 0.1f);
        glLoadIdentity();

        glMatrixMode(GL_COLOR);
        glTranslatef(time, 0, 0);
        DrawQuad(0.1f, 0.5f);
        glLoadIdentity();

        glMatrixMode(GL_COLOR);
        glTranslatef(0, time, 0);
        DrawQuad(0.5f, 0.5f);
        glLoadIdentity();

        glMatrixMode(GL_COLOR);
        glTranslatef(0, 0, time);
        DrawQuad(0.9f, 0.5f);
        glLoadIdentity();

        glMatrixMode(GL_COLOR);
        glScalef(time, time, time);
        DrawQuad(1.3f, 0.5f);
        glLoadIdentity();

        glMatrixMode(GL_COLOR);
        glRotatef(time * 1000, 1, 0, 0);
        DrawQuad(0.1f, 0.5f);
        glLoadIdentity();

#ifdef SDL2_BUILD
		SDL_GL_SwapWindow(win_handle);
#else
		glKosSwapBuffers();
#endif
        time += 0.001f;
	}
	return 0;
}
