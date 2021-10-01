#include <stdio.h>
#include <stdlib.h>

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"

#define TERRAIN_SIZE 100
#define TERRAIN_SCALE 1.0f
#define TERRAIN_HEIGHT_SCALE 1.0f

typedef struct {
    float x, y, z;
} Vec3;

Vec3 vertices[TERRAIN_SIZE * TERRAIN_SIZE];
unsigned int indices[TERRAIN_SIZE * TERRAIN_SIZE * 2];
Vec3 normals[TERRAIN_SIZE * TERRAIN_SIZE];

void GenerateTerrain() {
    const float OFFSET = (-TERRAIN_SIZE * TERRAIN_SCALE) / 2.0f;

    unsigned int j = 0;
    unsigned int z, x;
    for(z = 0; z < TERRAIN_SIZE; ++z) {
        for(x = 0; x < TERRAIN_SIZE; ++x) {
            unsigned int i = (z * TERRAIN_SIZE) + x;

            vertices[i].x = (x * TERRAIN_SCALE) + OFFSET;
            vertices[i].z = (z * TERRAIN_SCALE) + OFFSET;
            vertices[i].y = (float)rand() / (float)(RAND_MAX / TERRAIN_HEIGHT_SCALE);

            normals[i].x = normals[i].z = 0.0f;
            normals[i].y = 1.0f;

            if(z > 0) {
                indices[j++] = (i - TERRAIN_SIZE);
                indices[j++] = i;
            }
        }
    }
}

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);

    GenerateTerrain();

    /* glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glDisable(GL_BLEND);

    GLfloat mat_diffuse[] = {0.0, 1.0, 0.0, 1.0};
    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat mat_shininess[] = { 50.0 };
    GLfloat light_position[] = { 1.0, -1.0, 0.0, 0.0 };

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position); */

    fprintf(stderr, "Generated terrain\n");
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0)				// Prevent A Divide By Zero If The Window Is Too Small
        Height = 1;

    glViewport(0, 0, Width, Height);		// Reset The Current Viewport And Perspective Transformation

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);
    glMatrixMode(GL_MODELVIEW);
}

int check_start() {
#ifdef __DREAMCAST__
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    if(cont) {
        state = (cont_state_t *)maple_dev_status(cont);

        if(state)
            return state->buttons & CONT_START;
    }
#endif

    return 0;
}

/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glDisable(GL_CULL_FACE);

    glTranslatef(0.0f, -1.0f, 0.0f);

    static float x = 0.0f;
    x += 1.0f;

    glRotatef(x, 0, 1.0f, 0);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glNormalPointer(GL_FLOAT, 0, normals);
    glVertexPointer(3, GL_FLOAT, 0, &vertices[0].x);

    unsigned int z;
    for(z = 0; z < TERRAIN_SIZE; ++z) {
        glDrawElements(
            GL_TRIANGLE_STRIP,
            TERRAIN_SIZE * 2,
            GL_UNSIGNED_INT,
            indices + (z * (TERRAIN_SIZE * 2))
        );
    }

    // swap buffers to display, since we're double buffered.
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        if(check_start())
            break;

        DrawGLScene();
    }

    return 0;
}
