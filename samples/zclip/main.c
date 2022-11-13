#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glext.h"
#include "GL/glkos.h"

#ifdef __DREAMCAST__
#include <kos.h>
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#define IMG_FACADE00_PATH "/rd/facade00.tga"
#define IMG_FACADE01_PATH "/rd/facade01.tga"
#define IMG_FACADE02_PATH "/rd/facade02.tga"
#define IMG_FLOOR_PATH    "/rd/floor.tga"
#define IMG_LIGHTMAP_PATH "/rd/lightmap.tga"
#else
#define IMG_FACADE00_PATH "../samples/zclip/romdisk/facade00.tga"
#define IMG_FACADE01_PATH "../samples/zclip/romdisk/facade01.tga"
#define IMG_FACADE02_PATH "../samples/zclip/romdisk/facade02.tga"
#define IMG_FLOOR_PATH    "../samples/zclip/romdisk/floor.tga"
#define IMG_LIGHTMAP_PATH "../samples/zclip/romdisk/lightmap.tga"
#endif

/* Image type - contains height, width, and data */
typedef struct                                      // Create A Structure
{
    GLubyte *imageData;                             // Image Data (Up To 32 Bits)
    GLuint  bpp;                                    // Image Color Depth In Bits Per Pixel
    GLuint  width;                                  // Image Width
    GLuint  height;                                 // Image Height
    GLuint  texID;                                  // Texture ID Used To Select A Texture
} TextureImage;                                     // Structure Name


TextureImage textures[3];
TextureImage road;
TextureImage lightmap;


GLboolean LoadTGA(TextureImage *texture, const char *filename)			// Loads A TGA File Into Memory
{
    GLubyte		TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};	// Uncompressed TGA Header
    GLubyte		TGAcompare[12];								// Used To Compare TGA Header
    GLubyte		header[6];									// First 6 Useful Bytes From The Header
    GLuint		bytesPerPixel;								// Holds Number Of Bytes Per Pixel Used In The TGA File
    GLuint		imageSize;									// Used To Store The Image Size When Setting Aside Ram
    GLuint		temp;										// Temporary Variable
    GLuint		type=GL_RGBA;								// Set The Default GL Mode To RBGA (32 BPP)

    FILE *file = fopen(filename, "rb");						// Open The TGA File

    if(	file==NULL ||										// Does File Even Exist?
        fread(TGAcompare,1,sizeof(TGAcompare),file)!=sizeof(TGAcompare) ||	// Are There 12 Bytes To Read?
        memcmp(TGAheader,TGAcompare,sizeof(TGAheader))!=0				||	// Does The Header Match What We Want?
        fread(header,1,sizeof(header),file)!=sizeof(header))				// If So Read Next 6 Header Bytes
    {
        if (file == NULL) {								// Did The File Even Exist? *Added Jim Strong*
            fprintf(stderr, "Missing file\n");
            return GL_FALSE;									// Return False
        } else
        {
            fprintf(stderr, "Invalid format\n");
            fclose(file);									// If Anything Failed, Close The File
            return GL_FALSE;									// Return False
        }
    }

    texture->width  = header[1] * 256 + header[0];			// Determine The TGA Width	(highbyte*256+lowbyte)
    texture->height = header[3] * 256 + header[2];			// Determine The TGA Height	(highbyte*256+lowbyte)

    if(	texture->width	<=0	||								// Is The Width Less Than Or Equal To Zero
        texture->height	<=0	||								// Is The Height Less Than Or Equal To Zero
        (header[4]!=24 && header[4]!=32))					// Is The TGA 24 or 32 Bit?
    {
        fprintf(stderr, "Wrong format\n");
        fclose(file);										// If Anything Failed, Close The File
        return GL_FALSE;										// Return False
    }

    texture->bpp	= header[4];							// Grab The TGA's Bits Per Pixel (24 or 32)
    bytesPerPixel	= texture->bpp/8;						// Divide By 8 To Get The Bytes Per Pixel
    imageSize		= texture->width*texture->height*bytesPerPixel;	// Calculate The Memory Required For The TGA Data

    texture->imageData=(GLubyte *)malloc(imageSize);		// Reserve Memory To Hold The TGA Data

    if(	texture->imageData==NULL ||							// Does The Storage Memory Exist?
        fread(texture->imageData, 1, imageSize, file)!=imageSize)	// Does The Image Size Match The Memory Reserved?
    {
        if(texture->imageData!=NULL)						// Was Image Data Loaded
            free(texture->imageData);						// If So, Release The Image Data

        fclose(file);										// Close The File
        return GL_FALSE;										// Return False
    }

    GLuint i;
    for(i = 0; i < (int)imageSize; i += bytesPerPixel)		// Loop Through The Image Data
    {														// Swaps The 1st And 3rd Bytes ('R'ed and 'B'lue)
        temp=texture->imageData[i];							// Temporarily Store The Value At Image Data 'i'
        texture->imageData[i] = texture->imageData[i + 2];	// Set The 1st Byte To The Value Of The 3rd Byte
        texture->imageData[i + 2] = temp;					// Set The 3rd Byte To The Value In 'temp' (1st Byte Value)
    }

    fclose (file);											// Close The File

    // Build A Texture From The Data
    glGenTextures(1, &texture[0].texID);					// Generate OpenGL texture IDs

    glBindTexture(GL_TEXTURE_2D, texture[0].texID);			// Bind Our Texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	// Linear Filtered
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	// Linear Filtered

    if (texture[0].bpp==24)									// Was The TGA 24 Bits
    {
        type=GL_RGB;										// If So Set The 'type' To GL_RGB
    }

    glTexImage2D(GL_TEXTURE_2D, 0, type, texture[0].width, texture[0].height, 0, type, GL_UNSIGNED_BYTE, texture[0].imageData);

    return GL_TRUE;											// Texture Building Went Ok, Return True
}
// Load Bitmaps And Convert To Textures
void LoadGLTextures() {
    const char* filenames [] = {
        IMG_FACADE00_PATH,
        IMG_FACADE01_PATH,
        IMG_FACADE02_PATH
    };

    GLubyte i;
    for(i = 0; i < 3; ++i) {
        LoadTGA(&textures[i], filenames[i]);
    }

    if(!LoadTGA(&road, IMG_FLOOR_PATH)) {
        fprintf(stderr, "Error loading road texture");
    }

    if(!LoadTGA(&lightmap, IMG_LIGHTMAP_PATH)) {
        fprintf(stderr, "Error loading lightmap texture");
    }
}


/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    LoadGLTextures();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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

void RenderTower(int counter) {
    counter = counter % 3;
    float height = (counter + 1) * 5.0f;
    float width = 3.5f;

    float v = 1.0f * (counter + 1);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textures[counter].texID);

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, lightmap.texID);

    glBegin(GL_QUADS);
        glMultiTexCoord2f(GL_TEXTURE0, 0, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
        glVertex3f(-width, 0,-width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 0);
        glVertex3f(-width, 0, width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, v);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 1);
        glVertex3f(-width, height, width);
        glMultiTexCoord2f(GL_TEXTURE0, 0, v);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 1);
        glVertex3f(-width, height,-width);

        glMultiTexCoord2f(GL_TEXTURE0, 0, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
        glVertex3f(-width, 0, width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 0);
        glVertex3f( width, 0, width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, v);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 1);
        glVertex3f( width, height, width);
        glMultiTexCoord2f(GL_TEXTURE0, 0, v);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 1);
        glVertex3f(-width, height, width);

        glMultiTexCoord2f(GL_TEXTURE0, 0, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
        glVertex3f(width, 0,width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, 0);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 0);
        glVertex3f(width, 0,-width);
        glMultiTexCoord2f(GL_TEXTURE0, 1, v);
        glMultiTexCoord2f(GL_TEXTURE1, 1, 1);
        glVertex3f(width, height,-width);
        glMultiTexCoord2f(GL_TEXTURE0, 0, v);
        glMultiTexCoord2f(GL_TEXTURE1, 0, 1);
        glVertex3f(width, height, width);
    glEnd();
}


void RenderFloor() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, road.texID);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-100, 0, 0);
        glTexCoord2f(50.0f, 0.0f);
        glVertex3f( 100, 0, 0);
        glTexCoord2f(50.0f, 100.0f);
        glVertex3f( 100, 0, -200.0);
        glTexCoord2f(0.0f, 100.0f);
        glVertex3f(-100, 0, -200.0);
    glEnd();
}

/* The main drawing function. */
void DrawGLScene()
{
    static float z = 0.0f;
    static char increasing = 1;

    const float max = 50.0f;

    if(increasing) {
        z += 1.0f;
    } else {
        z -= 1.0f;
    }

    if(z > max) {
        increasing = !increasing;
        z = max;
    }

    if(z < 0.0f) {
        increasing = !increasing;
        z = 0.0f;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View
    glTranslatef(0, 0, z);

    GLubyte i = 0;

    glPushMatrix();
        glTranslatef(0.0f, -5.0f, 10.0f);
        RenderFloor();
    glPopMatrix();

    glPushMatrix();
        glTranslatef(-8.5f, -5.0f,-16.0f);		// Move Left 1.5 Units And Into The Screen 6.0

        for(i = 0; i < 10; ++i) {
            RenderTower(i);
            glTranslatef(0, 0, -8.0f);
        }
    glPopMatrix();

    glPushMatrix();
        glTranslatef(8.5f, -5.0f,-16.0f);		// Move Left 1.5 Units And Into The Screen 6.0

        for(i = 1; i < 11; ++i) {
            RenderTower(i);
            glTranslatef(0, 0, -8.0f);
        }
    glPopMatrix();

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
