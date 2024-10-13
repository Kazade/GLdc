#include <GL/gl.h>
#include <math.h>
#include <stdlib.h>

#ifndef __DREAMCAST__
#include <SDL2/SDL.h>
static SDL_Window* win_handle;
#else
#include <kos.h>
#include <GL/glkos.h>
#endif

static void DrawLineLoop(float y) {
	glBegin(GL_LINE_LOOP);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(410.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(490.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(490.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(410.0f, y + 90.0f);
	glEnd();
}

static void DrawLineStrip(float y) {
	glBegin(GL_LINE_STRIP);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(310.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(390.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(390.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(310.0f, y + 90.0f);
	glEnd();
}

static void DrawLine(float y) {
	glBegin(GL_LINES);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(210.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(290.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(290.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(210.0f, y + 90.0f);

	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(230.0f, y + 25.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(250.0f, y + 75.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(260.0f, y + 75.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(280.0f, y + 45.0f);
	glEnd();
}

static void DrawPoint(float y) {
	glBegin(GL_POINTS);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(110.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(190.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(190.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(110.0f, y + 90.0f);
	glEnd();
}

static void DrawQuad(float y) {
	glBegin(GL_QUADS);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(10.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(90.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(90.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(10.0f, y + 90.0f);

	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(110.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(190.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(190.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(110.0f, y + 90.0f);
	glEnd();
}

static void DrawQuadStrip(float y) {
	glBegin(GL_QUAD_STRIP);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(210.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(290.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(290.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(210.0f, y + 90.0f);

	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(310.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(390.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(390.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(310.0f, y + 90.0f);
	glEnd();
}

static void DrawTriList(float y) {
	glBegin(GL_TRIANGLES);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f( 10.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f( 90.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f( 90.0f, y + 90.0f);

	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(110.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(190.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(190.0f, y + 90.0f);
	glEnd();
}

static void DrawTriStrip(float y) {
	glBegin(GL_TRIANGLE_STRIP);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(210.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(290.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(290.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(210.0f, y + 90.0f);

	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(310.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(390.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(390.0f, y + 90.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(310.0f, y + 90.0f);
	glEnd();
}


static void DrawTriFan(float y) {
	glBegin(GL_TRIANGLE_FAN);
	glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(410.0f, y + 10.0f);
	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(490.0f, y + 10.0f);
	glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(445.0f, y + 90.0f);

	glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(590.0f, y + 10.0f);
	glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(510.0f, y + 45.0f);
	glEnd();
}

static void sample_init() {
#ifndef __DREAMCAST__
	SDL_Init(SDL_INIT_EVERYTHING);
	win_handle = SDL_CreateWindow("Shapes", 0, 0, 640, 480, SDL_WINDOW_OPENGL);
	SDL_GL_CreateContext(win_handle);
#else
    glKosInit();
#endif
}

static int sample_should_exit() {
#ifndef __DREAMCAST__
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

int main(int argc, char *argv[]) {
    sample_init();
	glClearColor(0.5f, 0.5f, 0.5f, 1);
	glViewport(0, 0, 640, 480);

	float mat[4][4] = { 0 };
	float L = 0, T = 0, R = 640, B = 480, N = -100, F = 100;
	mat[0][0] =  2.0f / (R - L);
	mat[1][1] =  2.0f / (T - B);
	mat[2][2] = -2.0f / (F - N);
	mat[3][0] = -(R + L) / (R - L);
	mat[3][1] = -(T + B) / (T - B);
	mat[3][2] = -(F + N) / (F - N);
	mat[3][3] = 1;
	glLoadMatrixf(*mat);

	while (!sample_should_exit()) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		DrawTriStrip(300);
		DrawTriFan(300);
		DrawTriList(300);

		DrawQuadStrip(200);
		DrawQuad(200);

		glPointSize(5);
		DrawPoint(100);
		glLineWidth(2);
		DrawLineLoop(100);
		glLineWidth(4);
		DrawLineStrip(100);
		glLineWidth(10);
		DrawLine(100);

		glPointSize(1);
		glLineWidth(1);
		DrawPoint(0);
		DrawLineLoop(0);
		DrawLineStrip(0);
		DrawLine(0);

		glShadeModel(GL_FLAT);
		DrawQuad(400);
		DrawTriStrip(400);
		glShadeModel(GL_SMOOTH);

#ifdef SDL2_BUILD
		SDL_GL_SwapWindow(win_handle);
#else
		glKosSwapBuffers();
#endif
	}
	return 0;
}
