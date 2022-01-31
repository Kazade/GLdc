#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"
#include "GL/glext.h"

#ifdef __DREAMCAST__
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#define IMG_PATH "/rd/NeHe.tex"
#else
#define IMG_PATH "../samples/nehe06_4444twid/romdisk/NeHe.tex"
#endif

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

/* storage for one texture  */
GLuint texture[1];

/* Image type - contains height, width, and data */
struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    char *data;
    GLenum format;
    GLenum internal_format;
    GLenum type;
    GLboolean mipmapped;
    unsigned int dataSize;
};

typedef struct Image Image;

int ImageLoad(char *filename, Image *image) {
    FILE* file = NULL;

    // make sure the file is there.
    if ((file = fopen(filename, "rb")) == NULL)
    {
        printf("File Not Found : %s\n",filename);
        return 0;
    }

    struct {
        char	id[4];	// 'DTEX'
        GLshort	width;
        GLshort	height;
        GLint		type;
        GLint		size;
    } header;

    fread(&header, sizeof(header), 1, file);

    GLboolean twiddled = (header.type & (1 << 25)) < 1;
    GLboolean compressed = (header.type & (1 << 29)) > 0;
    GLboolean mipmapped = (header.type & (1 << 30)) > 0;
    GLboolean strided = (header.type & (1 << 24)) > 0;
    GLuint format = (header.type >> 27) & 0b111;

    image->data = (char *) malloc (header.size);
    image->sizeX = header.width;
    image->sizeY = header.height;
    image->dataSize = header.size;

    GLuint expected = 2 * header.width * header.height;
    GLuint ratio = (GLuint) (((GLfloat) expected) / ((GLfloat) header.size));

    fread(image->data, image->dataSize, 1, file);
    fclose(file);

    printf("%d\n", compressed);
    printf("%d\n", twiddled);
    printf("%d\n", mipmapped);

    image->format = (format == 1) ? GL_RGB : GL_BGRA;
    image->internal_format = (format == 1) ? GL_RGB : GL_RGBA;

    GLuint COMPRESSED_MASK = 4;
    GLuint TWIDDLED_MASK = 2;
    GLuint MIPMAPPED_MASK = 1;

    GLuint lookup[8] = {0};

    switch(format) {
        case 0:
            lookup[COMPRESSED_MASK] = GL_COMPRESSED_ARGB_1555_VQ_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK] = GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS;
            lookup[COMPRESSED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS;
            lookup[TWIDDLED_MASK] = GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS;
            lookup[TWIDDLED_MASK | MIPMAPPED_MASK] = GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS;
            lookup[0] = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        break;
        case 1:
            lookup[COMPRESSED_MASK] = GL_COMPRESSED_RGB_565_VQ_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK] = GL_COMPRESSED_RGB_565_VQ_TWID_KOS;
            lookup[COMPRESSED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS;
            lookup[TWIDDLED_MASK] = GL_UNSIGNED_SHORT_5_6_5_TWID_KOS;
            lookup[TWIDDLED_MASK | MIPMAPPED_MASK] = GL_UNSIGNED_SHORT_5_6_5_TWID_KOS;
            lookup[0] = GL_UNSIGNED_SHORT_5_6_5;
        break;
        case 2:
            lookup[COMPRESSED_MASK] = GL_COMPRESSED_ARGB_4444_VQ_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK] = GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS;
            lookup[COMPRESSED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS;
            lookup[COMPRESSED_MASK | TWIDDLED_MASK | MIPMAPPED_MASK] = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS;
            lookup[TWIDDLED_MASK] = GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS;
            lookup[TWIDDLED_MASK | MIPMAPPED_MASK] = GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS;
            lookup[0] = GL_UNSIGNED_SHORT_4_4_4_4_REV;
        break;
        default:
            printf("[ERROR] Unknown format\n");
    }

    image->type = lookup[(compressed << 2) | (twiddled << 1) | mipmapped];

    printf("%d\n", image->type);

    assert(image->type);

    // we're done.
    return 1;
}

// Load Bitmaps And Convert To Textures
void LoadGLTextures() {
    // Load Texture
    Image *image1;

    // allocate space for texture
    image1 = (Image *) malloc(sizeof(Image));
    if (image1 == NULL) {
        printf("Error allocating space for image");
        exit(0);
    }

    if (!ImageLoad(IMG_PATH, image1)) {
        exit(1);
    }

    // Create Texture
    glGenTextures(1, &texture[0]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);   // 2d texture (x and y size)

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); // scale linearly when image smalled than texture

    // 2d texture, level of detail 0 (normal), 3 components (red, green, blue), x size from image, y size from image,
    // border 0 (normal), rgb color data, unsigned byte data, and finally the data itself.
    glTexImage2D(
        GL_TEXTURE_2D, 0, image1->internal_format, image1->sizeX, image1->sizeY, 0,
        image1->format, image1->type, image1->data
    );

    free(image1);
};

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    LoadGLTextures();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LESS);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);
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

    glTranslatef(0.0f,0.0f,-5.0f);              // move 5 units into the screen.

    glRotatef(xrot,1.0f,0.0f,0.0f);		// Rotate On The X Axis
    glRotatef(yrot,0.0f,1.0f,0.0f);		// Rotate On The Y Axis
    glRotatef(zrot,0.0f,0.0f,1.0f);		// Rotate On The Z Axis

    glBindTexture(GL_TEXTURE_2D, texture[0]);   // choose the texture to use.

    glBegin(GL_QUADS);		                // begin drawing a cube

    // Front Face (note that the texture's corners have to match the quad's corners)
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);	// Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);	// Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);	// Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);	// Top Left Of The Texture and Quad

    // Back Face
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);	// Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);	// Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);	// Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);	// Bottom Left Of The Texture and Quad

    // Top Face
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);	// Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);	// Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);	// Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);	// Top Right Of The Texture and Quad

    // Bottom Face
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);	// Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);	// Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);	// Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);	// Bottom Right Of The Texture and Quad

    // Right face
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);	// Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);	// Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);	// Top Left Of The Texture and Quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);	// Bottom Left Of The Texture and Quad

    // Left Face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);	// Bottom Left Of The Texture and Quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);	// Bottom Right Of The Texture and Quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);	// Top Right Of The Texture and Quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);	// Top Left Of The Texture and Quad

    glEnd();                                    // done with the polygon.

    xrot+=1.5f;		                // X Axis Rotation
    yrot+=1.5f;		                // Y Axis Rotation
    zrot+=1.5f; // Z Axis Rotation
             //
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
