/*
   KallistiOS 2.0.0

   nehe08.c
   (c)2021 Luke Benstead
   (c)2014 Josh Pearson
   (c)2001 Benoit Miller
   (c)2000 Jeff Molofee
*/

#ifdef __DREAMCAST__
#include <kos.h>
#else
#include <SDL.h>
#endif

#include <stdio.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glkos.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "../loadbmp.h"

#ifdef __DREAMCAST__
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#define IMG_PATH "/rd/brick.bmp"
#else
#define IMG_PATH "../samples/nehe10/romdisk/brick.bmp"
#endif

bool	keys[256];			// Array Used For The Keyboard Routine
bool	active = GL_TRUE;		// Window Active Flag Set To TRUE By Default
bool	fullscreen = GL_TRUE;	// Fullscreen Flag Set To Fullscreen Mode By Default
bool	blend;				// Blending ON/OFF
bool	bp;					// B Pressed?
bool	fp;					// F Pressed?

const float piover180 = 0.0174532925f;
float heading;
float xpos;
float zpos;

GLfloat	yrot;				// Y Rotation
GLfloat walkbias = 0;
GLfloat walkbiasangle = 0;
GLfloat lookupdown = 0.0f;
GLfloat	z=0.0f;				// Depth Into The Screen

GLuint	filter;				// Which Filter To Use
GLuint	texture[3];			// Storage For 3 Textures

typedef struct tagVERTEX
{
	float x, y, z;
	float u, v;
} VERTEX;

typedef struct tagTRIANGLE
{
	VERTEX vertex[3];
} TRIANGLE;

typedef struct tagSECTOR
{
	int numtriangles;
	TRIANGLE* triangle;
} SECTOR;

SECTOR sector1;

void readstr(FILE *f,char *string)
{
	do
	{
		fgets(string, 255, f);
	} while ((string[0] == '/') || (string[0] == '\n'));
	return;
}

void SetupWorld()
{
	float x, y, z, u, v;
	int numtriangles;
	FILE *filein;
	char oneline[255];
#ifdef __DREAMCAST__
	filein = fopen("/rd/world.txt", "rt");				// File To Load World Data From
#else
    filein = fopen("../samples/nehe10/romdisk/world.txt", "rt");
#endif

    if(!filein) {
        fprintf(stderr, "Failed to load world file\n");
        exit(1);
    }

	readstr(filein,oneline);
	sscanf(oneline, "NUMPOLLIES %d\n", &numtriangles);

	sector1.triangle = (TRIANGLE*) malloc(sizeof(TRIANGLE) * numtriangles);
	sector1.numtriangles = numtriangles;
	for (int loop = 0; loop < numtriangles; loop++)
	{
		for (int vert = 0; vert < 3; vert++)
		{
			readstr(filein,oneline);
			sscanf(oneline, "%f %f %f %f %f", &x, &y, &z, &u, &v);
			sector1.triangle[loop].vertex[vert].x = x;
			sector1.triangle[loop].vertex[vert].y = y;
			sector1.triangle[loop].vertex[vert].z = z;
			sector1.triangle[loop].vertex[vert].u = u;
			sector1.triangle[loop].vertex[vert].v = v;
		}
	}
	fclose(filein);
	return;
}

int LoadGLTextures()                                    // Load Bitmaps And Convert To Textures
{
    int Status = GL_FALSE;                               // Status Indicator

    Image image1;

    // Load The Bitmap, Check For Errors, If Bitmap's Not Found Quit
    if (ImageLoad(IMG_PATH, &image1))
    {
            Status = GL_TRUE;                            // Set The Status To TRUE

            glGenTextures(3, &texture[0]);          // Create Three Textures

            // Create Nearest Filtered Texture
            glBindTexture(GL_TEXTURE_2D, texture[0]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, 3, image1.sizeX, image1.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image1.data);

            // Create Linear Filtered Texture
            glBindTexture(GL_TEXTURE_2D, texture[1]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, 3, image1.sizeX, image1.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image1.data);

            // Create MipMapped Texture
            glBindTexture(GL_TEXTURE_2D, texture[2]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
            gluBuild2DMipmaps(GL_TEXTURE_2D, 3, image1.sizeX, image1.sizeY, GL_RGB, GL_UNSIGNED_BYTE, image1.data);
    }

    return Status;                                  // Return The Status
}

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
GLboolean InitGL(int width, int height)	        // We call this right after our OpenGL window is created.
{
    glViewport(0, 0, width, height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();

	if (!LoadGLTextures())								// Jump To Texture Loading Routine
	{
		return GL_FALSE;									// If Texture Didn't Load Return false
	}

	glEnable(GL_TEXTURE_2D);							// Enable Texture Mapping
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);					// Set The Blending Function For Translucency
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);				// This Will Clear The Background Color To Black
	glClearDepth(1.0);									// Enables Clearing Of The Depth Buffer
	glDepthFunc(GL_LESS);								// The Type Of Depth Test To Do
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glShadeModel(GL_SMOOTH);							// Enables Smooth Color Shading
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations

	SetupWorld();

	return GL_TRUE;
}

void DrawGLScene(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
	glLoadIdentity();									// Reset The View

	GLfloat x_m, y_m, z_m, u_m, v_m;
	GLfloat xtrans = -xpos;
	GLfloat ztrans = -zpos;
	GLfloat ytrans = -walkbias-0.25f;
	GLfloat sceneroty = 360.0f - yrot;

	int numtriangles;

	glRotatef(lookupdown,1.0f,0,0);
	glRotatef(sceneroty,0,1.0f,0);

	glTranslatef(xtrans, ytrans, ztrans);
	glBindTexture(GL_TEXTURE_2D, texture[filter]);

	numtriangles = sector1.numtriangles;

	// Process Each Triangle
	for (int loop_m = 0; loop_m < numtriangles; loop_m++)
	{
		glBegin(GL_TRIANGLES);
			glNormal3f( 0.0f, 0.0f, 1.0f);
			x_m = sector1.triangle[loop_m].vertex[0].x;
			y_m = sector1.triangle[loop_m].vertex[0].y;
			z_m = sector1.triangle[loop_m].vertex[0].z;
			u_m = sector1.triangle[loop_m].vertex[0].u;
			v_m = sector1.triangle[loop_m].vertex[0].v;
			glTexCoord2f(u_m,v_m); glVertex3f(x_m,y_m,z_m);

			x_m = sector1.triangle[loop_m].vertex[1].x;
			y_m = sector1.triangle[loop_m].vertex[1].y;
			z_m = sector1.triangle[loop_m].vertex[1].z;
			u_m = sector1.triangle[loop_m].vertex[1].u;
			v_m = sector1.triangle[loop_m].vertex[1].v;
			glTexCoord2f(u_m,v_m); glVertex3f(x_m,y_m,z_m);

			x_m = sector1.triangle[loop_m].vertex[2].x;
			y_m = sector1.triangle[loop_m].vertex[2].y;
			z_m = sector1.triangle[loop_m].vertex[2].z;
			u_m = sector1.triangle[loop_m].vertex[2].u;
			v_m = sector1.triangle[loop_m].vertex[2].v;
			glTexCoord2f(u_m,v_m); glVertex3f(x_m,y_m,z_m);
		glEnd();
	}
    /* Finish the frame */
    glKosSwapBuffers();
}

int ReadController(void) {
    bool start = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;


#ifdef __DREAMCAST__
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

    /* Check key status */
    state = (cont_state_t *)maple_dev_status(cont);
    if(!state) {
        printf("Error reading controller\n");
        return 0;
    }

    start = (state->buttons & CONT_START);
    up = (state->buttons & CONT_DPAD_UP);
    down = (state->buttons & CONT_DPAD_DOWN);
    left = (state->buttons & CONT_DPAD_LEFT);
    right = (state->buttons & CONT_DPAD_RIGHT);

#else
    int num_keys = 0;
    uint8_t* state = SDL_GetKeyboardState(&num_keys);
    start = state[SDL_SCANCODE_RETURN];
    up = state[SDL_SCANCODE_UP];
    down = state[SDL_SCANCODE_DOWN];
    left = state[SDL_SCANCODE_LEFT];
    right = state[SDL_SCANCODE_RIGHT];
#endif

    if(start) {
        return 0;
    }

    if(up) {
        xpos -= (float)sin(heading*piover180) * 0.05f;
        zpos -= (float)cos(heading*piover180) * 0.05f;
        if (walkbiasangle >= 359.0f)
        {
            walkbiasangle = 0.0f;
        }
        else
        {
            walkbiasangle+= 10;
        }
        walkbias = (float)sin(walkbiasangle * piover180)/20.0f;
    }

    if(down) {
        xpos += (float)sin(heading*piover180) * 0.05f;
        zpos += (float)cos(heading*piover180) * 0.05f;
        if (walkbiasangle <= 1.0f)
        {
            walkbiasangle = 359.0f;
        }
        else
        {
            walkbiasangle-= 10;
        }
        walkbias = (float)sin(walkbiasangle * piover180)/20.0f;
    }

    if(left) {
        heading += 1.0f;
        yrot = heading;
    }

    if(right) {
        heading -= 1.0f;
        yrot = heading;
    }



    /* Switch to the blended polygon list if needed */
    if(blend) {
        glEnable(GL_BLEND);
        glDepthMask(0);
    }
    else {
        glDisable(GL_BLEND);
        glDepthMask(1);
    }

    return 1;
}

int main(int argc, char **argv) {
    printf("nehe10 beginning\n");

    /* Get basic stuff initialized */
    glKosInit();

    InitGL(640, 480);

    while(1) {
        if (!ReadController())
            break;

        DrawGLScene();
    }

    return 0;
}
