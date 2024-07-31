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
#define IMG_PATH "../samples/nehe06_vq/romdisk/NeHe.tex"
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
    GLenum internalFormat;
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
        GLushort	width;
        GLushort	height;
        GLuint		type;
        GLuint		size;
    } header;

    fread(&header, sizeof(header), 1, file);

    GLboolean twiddled = (header.type & (1 << 26)) < 1;
    GLboolean compressed = (header.type & (1 << 30)) > 0;
    GLboolean mipmapped = (header.type & (1 << 31)) > 0;
    //GLboolean strided = (header.type & (1 << 25)) > 0;
    GLuint format = (header.type >> 27) & 0b111;

    image->data = (char *) malloc (header.size);
    image->sizeX = header.width;
    image->sizeY = header.height;
    image->dataSize = header.size;

    //GLuint expected = 2 * header.width * header.height;
    //GLuint ratio = (GLuint) (((GLfloat) expected) / ((GLfloat) header.size));

    fread(image->data, image->dataSize, 1, file);
    fclose(file);

    if(compressed) {
        if(twiddled) {
            switch(format) {
                case 0: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS;
                    }
                } break;
                case 1: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_RGB_565_VQ_TWID_KOS;
                    }
                } break;
                case 2: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS;
                    }
                }
                break;
                default:
                    fprintf(stderr, "Invalid texture format");
                    return 0;
            }
        } else {
            switch(format) {
                case 0: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_KOS;
                    }
                } break;
                case 1: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_RGB_565_VQ_KOS;
                    }
                } break;
                case 2: {
                    if(mipmapped) {
                        image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS;
                    } else {
                        image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_KOS;
                    }
                }
                break;
                default:
                    fprintf(stderr, "Invalid texture format");
                    return 0;
            }
        }
    } else {
        printf("Not a compressed texture");
        return 0;
    }

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
    glCompressedTexImage2DARB(
        GL_TEXTURE_2D, 0, image1->internalFormat, image1->sizeX, image1->sizeY, 0,
        image1->dataSize, image1->data
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
