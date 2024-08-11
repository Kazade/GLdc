/*  DREAMCAST
 *IAN MICHEAL Ported SDL+OPENGL USING SDL[DREAMHAL][GLDC][KOS2.0]2021
 * Cleaned and tested on dreamcast hardware by Ianmicheal
 *		This Code Was Created By Pet & Commented/Cleaned Up By Jeff Molofee
 *		If You've Found This Code Useful, Please Let Me Know.
 *		Visit NeHe Productions At http://nehe.gamedev.net
 */

#include <math.h>			// Header File For Windows Math Library
#include <stdio.h>			// Header File For Standard Input/Output
#include <stdint.h>
#include <stdlib.h>


#ifdef __DREAMCAST__
#include <kos.h>
#endif

#define FPS 60
uint32_t waittime = 1000.0f/FPS;
uint32_t framestarttime = 0;
int32_t delaytime;

#ifdef __DREAMCAST__
extern uint8_t romdisk[];
KOS_INIT_ROMDISK(romdisk);
#define IMG_LOGO_PATH   "/rd/logo.bmp"
#define IMG_MASK1_PATH  "/rd/mask1.bmp"
#define IMG_IMAGE1_PATH "/rd/image1.bmp"
#define IMG_MASK2_PATH  "/rd/mask2.bmp"
#define IMG_IMAGE2_PATH "/rd/image2.bmp"
#else
#define IMG_LOGO_PATH   "../samples/nehe20/romdisk/logo.bmp"
#define IMG_MASK1_PATH  "../samples/nehe20/romdisk/mask1.bmp"
#define IMG_IMAGE1_PATH "../samples/nehe20/romdisk/image1.bmp"
#define IMG_MASK2_PATH  "../samples/nehe20/romdisk/mask2.bmp"
#define IMG_IMAGE2_PATH "../samples/nehe20/romdisk/image2.bmp"
#endif

#include "../loadbmp.h"

/*
 *		This Code Was Created By Jeff Molofee 2000
 *		And Modified By Giuseppe D'Agata (waveform@tiscalinet.it)
 *		If You've Found This Code Useful, Please Let Me Know.
 *		Visit My Site At nehe.gamedev.net
 */

#include <math.h>			// Header File For Windows Math Library
#include <stdio.h>			// Header File For Standard Input/Output
#include <assert.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/gl.h>	// Header File For The OpenGL32 Library
#include <OpenGL/glu.h>	// Header File For The GLu32 Library
#elif defined(__DREAMCAST__)
#include <kos.h>
#include <GL/gl.h>	// Header File For The OpenGL32 Library
#include <GL/glu.h>	// Header File For The GLu32 Library
#include <GL/glkos.h>
#else
#include <GL/gl.h>	// Header File For The OpenGL32 Library
#include <GL/glu.h>	// Header File For The GLu32 Library
#endif

#define BOOL    int
#define FALSE   0
#define TRUE    1


uint8_t*	keys;			// Array Used For The Keyboard Routine
BOOL	active=TRUE;		// Window Active Flag Set To TRUE By Default
BOOL	fullscreen=FALSE;	// Fullscreen Flag Set To Fullscreen Mode By Default
BOOL	masking=TRUE;		// Masking On/Off
BOOL	mp;					// M Pressed?
BOOL	sp;					// Space Pressed?
BOOL	scene;				// Which Scene To Draw

GLuint	texture[5];			// Storage For Our Five Textures
GLuint	loop;				// Generic Loop Variable

GLfloat	roll;				// Rolling Texture

int LoadGLTextures()                                    // Load Bitmaps And Convert To Textures
{
        int Status=FALSE;                               // Status Indicator

		Image TextureImage[5];

        if ((ImageLoad(IMG_LOGO_PATH, &TextureImage[0])) &&	// Logo Texture
			(ImageLoad(IMG_MASK1_PATH, &TextureImage[1])) &&	// First Mask
			(ImageLoad(IMG_IMAGE1_PATH, &TextureImage[2])) &&	// First Image
			(ImageLoad(IMG_MASK2_PATH, &TextureImage[3])) &&	// Second Mask
			(ImageLoad(IMG_IMAGE2_PATH, &TextureImage[4])))	// Second Image
        {
                Status=TRUE;                            // Set The Status To TRUE
                glGenTextures(5, &texture[0]);          // Create Five Textures

				for (loop=0; loop<5; loop++)			// Loop Through All 5 Textures
				{
	                glBindTexture(GL_TEXTURE_2D, texture[loop]);
			        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexImage2D(
						GL_TEXTURE_2D, 0, 3,
						TextureImage[loop].sizeX,
						TextureImage[loop].sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE,
						TextureImage[loop].data
					);
				}
        }

        return Status;                                  // Return The Status
}

GLvoid ReSizeGLScene(GLsizei width, GLsizei height)		// Resize And Initialize The GL Window
{
	if (height==0)										// Prevent A Divide By Zero By
	{
		height=1;										// Making Height Equal One
	}

	glViewport(0,0,width,height);						// Reset The Current Viewport
	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix
	gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);	// Calculate Window Aspect Ratio
	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}

int InitGL(GLvoid)										// All Setup For OpenGL Goes Here
{
	if (!LoadGLTextures())								// Jump To Texture Loading Routine
	{
		return FALSE;									// If Texture Didn't Load Return FALSE
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);				// Clear The Background Color To Black
	glClearDepth(1.0);									// Enables Clearing Of The Depth Buffer
	glEnable(GL_DEPTH_TEST);							// Enable Depth Testing
	glShadeModel(GL_SMOOTH);							// Enables Smooth Color Shading
	glEnable(GL_TEXTURE_2D);							// Enable 2D Texture Mapping
	return TRUE;										// Initialization Went OK
}

int DrawGLScene(GLvoid)									// Here's Where We Do All The Drawing
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
	glLoadIdentity();									// Reset The Modelview Matrix
	glTranslatef(0.0f,0.0f,-2.0f);						// Move Into The Screen 5 Units

	glBindTexture(GL_TEXTURE_2D, texture[0]);			// Select Our Logo Texture
	glBegin(GL_QUADS);									// Start Drawing A Textured Quad
		glTexCoord2f(0.0f, -roll+0.0f); glVertex3f(-1.1f, -1.1f,  0.0f);	// Bottom Left
		glTexCoord2f(3.0f, -roll+0.0f); glVertex3f( 1.1f, -1.1f,  0.0f);	// Bottom Right
		glTexCoord2f(3.0f, -roll+3.0f); glVertex3f( 1.1f,  1.1f,  0.0f);	// Top Right
		glTexCoord2f(0.0f, -roll+3.0f); glVertex3f(-1.1f,  1.1f,  0.0f);	// Top Left
	glEnd();											// Done Drawing The Quad

	glEnable(GL_BLEND);									// Enable Blending
	glDisable(GL_DEPTH_TEST);							// Disable Depth Testing

	if (masking)										// Is Masking Enabled?
	{
		glBlendFunc(GL_DST_COLOR,GL_ZERO);				// Blend Screen Color With Zero (Black)
	}

	if (scene)											// Are We Drawing The Second Scene?
	{
		glTranslatef(0.0f,0.0f,-1.0f);					// Translate Into The Screen One Unit
		glRotatef(roll*360,0.0f,0.0f,1.0f);				// Rotate On The Z Axis 360 Degrees.
		if (masking)									// Is Masking On?
		{
			glBindTexture(GL_TEXTURE_2D, texture[3]);	// Select The Second Mask Texture
			glBegin(GL_QUADS);							// Start Drawing A Textured Quad
				glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.1f, -1.1f,  0.0f);	// Bottom Left
				glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.1f, -1.1f,  0.0f);	// Bottom Right
				glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.1f,  1.1f,  0.0f);	// Top Right
				glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.1f,  1.1f,  0.0f);	// Top Left
			glEnd();									// Done Drawing The Quad
		}

		glBlendFunc(GL_ONE, GL_ONE);					// Copy Image 2 Color To The Screen
		glBindTexture(GL_TEXTURE_2D, texture[4]);		// Select The Second Image Texture
		glBegin(GL_QUADS);								// Start Drawing A Textured Quad
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.1f, -1.1f,  0.0f);	// Bottom Left
			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.1f, -1.1f,  0.0f);	// Bottom Right
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.1f,  1.1f,  0.0f);	// Top Right
			glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.1f,  1.1f,  0.0f);	// Top Left
		glEnd();										// Done Drawing The Quad
	}
	else												// Otherwise
	{
		if (masking)									// Is Masking On?
		{
			glBindTexture(GL_TEXTURE_2D, texture[1]);	// Select The First Mask Texture
			glBegin(GL_QUADS);							// Start Drawing A Textured Quad
				glTexCoord2f(roll+0.0f, 0.0f); glVertex3f(-1.1f, -1.1f,  0.0f);	// Bottom Left
				glTexCoord2f(roll+4.0f, 0.0f); glVertex3f( 1.1f, -1.1f,  0.0f);	// Bottom Right
				glTexCoord2f(roll+4.0f, 4.0f); glVertex3f( 1.1f,  1.1f,  0.0f);	// Top Right
				glTexCoord2f(roll+0.0f, 4.0f); glVertex3f(-1.1f,  1.1f,  0.0f);	// Top Left
			glEnd();									// Done Drawing The Quad
		}

		glBlendFunc(GL_ONE, GL_ONE);					// Copy Image 1 Color To The Screen
		glBindTexture(GL_TEXTURE_2D, texture[2]);		// Select The First Image Texture
		glBegin(GL_QUADS);								// Start Drawing A Textured Quad
			glTexCoord2f(roll+0.0f, 0.0f); glVertex3f(-1.1f, -1.1f,  0.0f);	// Bottom Left
			glTexCoord2f(roll+4.0f, 0.0f); glVertex3f( 1.1f, -1.1f,  0.0f);	// Bottom Right
			glTexCoord2f(roll+4.0f, 4.0f); glVertex3f( 1.1f,  1.1f,  0.0f);	// Top Right
			glTexCoord2f(roll+0.0f, 4.0f); glVertex3f(-1.1f,  1.1f,  0.0f);	// Top Left
		glEnd();										// Done Drawing The Quad
	}

	glEnable(GL_DEPTH_TEST);							// Enable Depth Testing
	glDisable(GL_BLEND);								// Disable Blending

	roll+=0.002f;										// Increase Our Texture Roll Variable
	if (roll>1.0f)										// Is Roll Greater Than One
	{
		roll-=1.0f;										// Subtract 1 From Roll
	}

    glKosSwapBuffers();

	return TRUE;										// Everything Went OK
}

int main(int argc, char *argv[])
{
    glKosInit();

    InitGL();
    ReSizeGLScene(640, 480);

#ifdef __DREAMCAST__
	maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	assert(cont);
#endif

    while(1) {
        DrawGLScene();

#ifdef __DREAMCAST__
		cont_state_t* state = (cont_state_t *)maple_dev_status(cont);

		if((state->buttons & CONT_A) && !sp) {
			sp = TRUE;
			scene = !scene;
		} else {
			sp = FALSE;
		}

		if((state->buttons & CONT_B) && !mp) {
			mp = TRUE;
			masking = !masking;
		} else {
			mp = FALSE;
		}

		if(state->buttons & CONT_START) {
			break;
		}
#endif
    }

	return 0;
}
