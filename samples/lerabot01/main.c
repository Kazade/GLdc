#include <stdio.h>
#include "gl.h"
#include "glu.h"
#include "glkos.h"
#include <time.h>

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

/* storage for one texture  */
int texture[1];

/* Image type - contains height, width, and data */
struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    char *data;
};
typedef struct Image Image;

// quick and dirty bitmap loader...for 24 bit bitmaps with 1 plane only.
// See http://www.dcs.ed.ac.uk/~mxr/gfx/2d/BMP.txt for more info.
int ImageLoad(char *filename, Image *image) {
    FILE *file;
    unsigned long size;                 // size of the image in bytes.
    unsigned long i;                    // standard counter.
    unsigned short int planes;          // number of planes in image (must be 1)
    unsigned short int bpp;             // number of bits per pixel (must be 24)
    char temp;                          // temporary color storage for bgr-rgb conversion.

    // make sure the file is there.
    if ((file = fopen(filename, "rb"))==NULL)
    {
        printf("File Not Found : %s\n",filename);
        return 0;
    }

    // seek through the bmp header, up to the width/height:
    fseek(file, 18, SEEK_CUR);

    // read the width
    if ((i = fread(&image->sizeX, 4, 1, file)) != 1) {
        printf("Error reading width from %s.\n", filename);
        return 0;
    }
    printf("Width of %s: %lu\n", filename, image->sizeX);

    // read the height
    if ((i = fread(&image->sizeY, 4, 1, file)) != 1) {
        printf("Error reading height from %s.\n", filename);
        return 0;
    }
    printf("Height of %s: %lu\n", filename, image->sizeY);

    // calculate the size (assuming 24 bits or 3 bytes per pixel).
    size = image->sizeX * image->sizeY * 3;

    // read the planes
    if ((fread(&planes, 2, 1, file)) != 1) {
        printf("Error reading planes from %s.\n", filename);
        return 0;
    }
    if (planes != 1) {
        printf("Planes from %s is not 1: %u\n", filename, planes);
        return 0;
    }

    // read the bpp
    if ((i = fread(&bpp, 2, 1, file)) != 1) {
        printf("Error reading bpp from %s.\n", filename);
        return 0;
    }
    if (bpp != 24) {
        printf("Bpp from %s is not 24: %u\n", filename, bpp);
        return 0;
    }

    // seek past the rest of the bitmap header.
    fseek(file, 24, SEEK_CUR);

    // read the data.
    image->data = (char *) malloc(size);
    if (image->data == NULL) {
        printf("Error allocating memory for color-corrected image data");
        return 0;
    }

    if ((i = fread(image->data, size, 1, file)) != 1) {
        printf(stderr, "Error reading image data from %s.\n", filename);
        return 0;
    }

    for (i=0;i<size;i+=3) { // reverse all of the colors. (bgr -> rgb)
        temp = image->data[i];
        image->data[i] = image->data[i+2];
        image->data[i+2] = temp;
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

    if (!ImageLoad("/rd/flag1.bmp", image1)) {
        exit(1);
    }

    // Create Texture
    glGenTextures(1, &texture[0]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);   // 2d texture (x and y size)

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); // scale linearly when image smalled than texture

    // 2d texture, level of detail 0 (normal), 3 components (red, green, blue), x size from image, y size from image,
    // border 0 (normal), rgb color data, unsigned byte data, and finally the data itself.
    glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->sizeX, image1->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image1->data);
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
    glEnable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    GLfloat l1_pos[] = {5.0, 0.0, 1.0, 1.0};
    GLfloat l1_diff[] = {1.0, 0.0, 0.0, 1.0};
    GLfloat l1_amb[] = {0.5, 0.5, 0.5, 1.0};

    //glLightfv(GL_LIGHT1, GL_AMBIENT,  l1_amb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  l1_diff);
    glLightfv(GL_LIGHT1, GL_POSITION, l1_pos);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.0001);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.00001);

    GLfloat l2_pos[] = {0.0, 15.0, 1.0, 1.0};
    GLfloat l2_dir[] = {0.0, -1.0, 0.0};
    GLfloat l2_diff[] = {0.5, 0.5, 0.0, 1.0};
    GLfloat l2_amb[] = {0.5, 0.5, 0.5, 1.0};

    glEnable(GL_LIGHT2);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, l2_diff);
    glLightfv(GL_LIGHT2, GL_POSITION, l2_pos);
    glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, l2_dir);
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.0001);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.00001);
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

void DrawTexturedQuad(int tex, float x, float y, float z)
{
  GLfloat texW = 10;
  GLfloat texH = 10;
  GLfloat x0 = x - texW / 2;
  GLfloat y0 = y - texH / 2;
  GLfloat x1 = x + texW / 2;
  GLfloat y1 = y + texH / 2;
  GLfloat color[] = {1.0f, 1.0f, 1.0f, 1.0f};
  GLfloat mat_ambient[] = {1.0f, 1.0f, 1.0f, 1.0f};

  GLfloat vertex_data[] = {
  	/* 2D Coordinate, texture coordinate */
  	x0, y1, z,
  	x1, y1, z,
  	x1, y0, z,
  	x0, y0, z
  };

  GLfloat uv_data[] = {
  	/* 2D Coordinate, texture coordinate */
  	0.0f, 1.0f,
  	1.0f, 1.0f,
  	1.0f, 0.0f,
  	0.0f, 0.0f
  };

  GLfloat normal_data[] = {
  	/* 2D Coordinate, texture coordinate */
  	0.0, 0.0, 1.0,
  	0.0, 0.0, 1.0,
  	0.0, 0.0, 1.0,
  	0.0, 0.0, 1.0
  };

  GLfloat color_data[] = {
  	/* 2D Coordinate, texture coordinate */
  	color[0], color[1], color[2], color[3],
  	color[0], color[1], color[2], color[3],
  	color[0], color[1], color[2], color[3],
  	color[0], color[1], color[2], color[3]
  };

  //GLint indices[] = {0,1,2,3,2,3};

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  //glEnableClientState(GL_COLOR_ARRAY);

  glVertexPointer(3, GL_FLOAT, 0, vertex_data);
  glTexCoordPointer(2, GL_FLOAT, 0, uv_data);
  glNormalPointer(GL_FLOAT, 0, normal_data);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_ambient);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glVertexPointer(3, GL_FLOAT, 0, vertex_data);
  glDrawArrays(GL_QUADS, 0, 4);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  //glDisableClientState(GL_COLOR_ARRAY);
}


float delta = 0;
/* The main drawing function. */
void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glTranslatef(-50.0f, 0.0f, -100.0f);

    GLfloat l1_pos[] = {50 + sin(delta) * 100.0f, 6.0, 5.0, 1.0};
    delta += 0.03;

    glLightfv(GL_LIGHT1, GL_POSITION, l1_pos);
    //glLightfv(GL_LIGHT1, GL_SPOT_EXPONENT, 3);
    DrawTexturedQuad(texture[0], l1_pos[0], l1_pos[1], l1_pos[2]);

    for (int i = 0; i < 5; i++)
      DrawTexturedQuad(texture[0], i * 20, 0.0f, 0.0f); // Draw the textured quad.
    // swap buffers to display, since we're double buffered.
    glKosSwapBuffers();

}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        DrawGLScene();
    }

    return 0;
}
