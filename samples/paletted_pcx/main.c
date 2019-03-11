#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "gl.h"
#include "glext.h"
#include "glu.h"
#include "glkos.h"

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

int textures[3];

typedef struct {
    unsigned int height;
    unsigned int width;
    unsigned int palette_width;
    char* palette;
    char* data;
} Image;

#pragma pack(push)
#pragma pack(1)

/* PCX header */
typedef struct {
    uint8_t manufacturer;
    uint8_t version;
    uint8_t encoding;
    uint8_t bits_per_pixel;

    uint16_t xmin, ymin;
    uint16_t xmax, ymax;
    uint16_t horz_res, vert_res;

    uint8_t palette[48];
    uint8_t reserved;
    uint8_t num_color_planes;

    uint16_t bytes_per_scan_line;
    uint16_t palette_type;
    uint16_t horz_size, vert_size;

    uint8_t padding[54];
} Header;

#pragma pack(pop)

int LoadPalettedPCX(const char* filename, Image* image) {
    FILE* filein = NULL;
    filein = fopen(filename, "rb");
    if(!filein) {
        printf("Unable to open file\n");
        return 0;
    }

    Header header;

    fread(&header, sizeof(header), 1, filein);

    if(header.manufacturer != 0x0a) {
        printf("Invalid file\n");
        return 0;
    }

    /* Read the rest of the file */
    long pos = ftell(filein);
    fseek(filein, 0, SEEK_END);
    long end = ftell(filein);
    fseek(filein, pos, SEEK_SET);

    long size = end - pos;
    uint8_t* buffer = (uint8_t*) malloc(size);
    fread(buffer, 1, size, filein);

    image->width = header.xmax - header.xmin + 1;
    image->height = header.ymax - header.ymin + 1;
    image->data = (char*) malloc(sizeof(char) * image->width * image->height);

    int bitcount = header.bits_per_pixel * header.num_color_planes;
    if(bitcount != 8) {
        printf("Wrong bitcount\n");
        return 0;
    }

    uint8_t palette_marker = buffer[size - 769];

    const uint8_t* palette = (palette_marker == 12) ? &buffer[size - 768] : header.palette;

    image->palette_width = (palette_marker == 12) ? 256 : 16;
    image->palette = (char*) malloc(sizeof(char) * 3 * image->palette_width);
    memcpy(image->palette, palette, sizeof(char) * 3 * image->palette_width);

    int32_t rle_count = 0;
    int32_t rle_value = 0;

    const uint8_t* image_data = buffer;

    uint32_t idx;
    for(idx = 0; idx < (image->width * image->height); idx++) {
        if(rle_count == 0) {
            rle_value = *image_data;
            ++image_data;
            if((rle_value & 0xc0) == 0xc0) {
                rle_count = rle_value & 0x3f;
                rle_value = *image_data;
                ++image_data;
            } else {
                rle_count = 1;
            }
        }

        rle_count--;

        assert(rle_value < 256);

        image->data[idx] = rle_value;
    }

    free(buffer);

    return 1;
}

// Load Bitmaps And Convert To Textures
void LoadGLTextures() {
    // Load Texture
    Image image1, image2;

    if(!LoadPalettedPCX("/rd/NeHe.pcx", &image1)) {
        exit(1);
    }

    if(!LoadPalettedPCX("/rd/NeHe-Alpha.pcx", &image2)) {
        exit(1);
    }

    glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);

    /* First palette */
    glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGBA8, image1.palette_width, GL_RGB, GL_UNSIGNED_BYTE, image1.palette);

    char* inversed_palette = (char*) malloc(sizeof(char) * image1.palette_width * 3);
    GLuint i;
    for(i = 0; i < image1.palette_width; i++) {
        /* Swap red and green */
        inversed_palette[i * 3] = image1.palette[(i * 3) + 1];
        inversed_palette[(i * 3) + 1] = image1.palette[(i * 3)];
        inversed_palette[(i * 3) + 2] = image1.palette[(i * 3) + 2];
    }

    glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_1_KOS, GL_RGBA8, image1.palette_width, GL_RGB, GL_UNSIGNED_BYTE, inversed_palette);

    // Create Texture
    glGenTextures(3, textures);
    glBindTexture(GL_TEXTURE_2D, textures[0]);   // 2d texture (x and y size)

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR); // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR); // scale linearly when image smalled than texture

    // 2d texture, level of detail 0 (normal), 3 components (red, green, blue), x size from image, y size from image,
    // border 0 (normal), rgb color data, unsigned byte data, and finally the data itself.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image1.data);

    glBindTexture(GL_TEXTURE_2D, textures[1]);   // 2d texture (x and y size)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR); // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR); // scale linearly when image smalled than texture

    /* Texture-specific palette! */
    glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, image1.palette_width, GL_RGB, GL_UNSIGNED_BYTE, inversed_palette);

    // 2d texture, level of detail 0 (normal), 3 components (red, green, blue), x size from image, y size from image,
    // border 0 (normal), rgb color data, unsigned byte data, and finally the data itself.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image1.data);

    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    char* new_palette = (char*) malloc(image2.palette_width * 4);
    for(i = 0; i < image2.palette_width; ++i) {
        new_palette[(i * 4) + 0] = image2.palette[(i * 3) + 0];
        new_palette[(i * 4) + 1] = image2.palette[(i * 3) + 1];
        new_palette[(i * 4) + 2] = image2.palette[(i * 3) + 2];
        new_palette[(i * 4) + 3] = (i == 2) ? 0 : 255;
    }

    glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, image2.palette_width, GL_RGBA, GL_UNSIGNED_BYTE, new_palette);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image2.width, image2.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image2.data);
}

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    LoadGLTextures();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
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

void DrawPolygon() {
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
}

/* The main drawing function. */
void DrawGLScene()
{
    static GLuint switch_counter = 0;
    static GLuint current_bank = 0;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);

    glTranslatef(-1.5f,0.0f,-8.0f);              // move 5 units into the screen.

    glPushMatrix();
        glRotatef(xrot,1.0f,0.0f,0.0f);		// Rotate On The X Axis
        glRotatef(yrot,0.0f,1.0f,0.0f);		// Rotate On The Y Axis
        glRotatef(zrot,0.0f,0.0f,1.0f);		// Rotate On The Z Axis
        glBindTexture(GL_TEXTURE_2D, textures[0]);   // choose the texture to use.

        if(switch_counter++ > 200) {
            switch_counter = 0;
            current_bank = !current_bank;
            glTexParameteri(GL_TEXTURE_2D, GL_SHARED_TEXTURE_BANK_KOS, current_bank);
        }

        DrawPolygon();
    glPopMatrix();

    glDisable(GL_SHARED_TEXTURE_PALETTE_EXT);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTranslatef(3.0, 0, 0);
    DrawPolygon();

    static float x = 0.0f;
    x += 0.05f;
    if(x > 5.0f) {
        x = 0.0f;
    }

    glAlphaFunc(GL_GREATER, 0.666f);
    glEnable(GL_ALPHA_TEST);
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTranslatef(x - 3.0, 0, 3.0);
    DrawPolygon();
    glDisable(GL_ALPHA_TEST);

    xrot+=1.5f;		                // X Axis Rotation
    yrot+=1.5f;		                // Y Axis Rotation
    zrot+=1.5f; // Z Axis Rotation
             //
    // swap buffers to display, since we're double buffered.
    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    GLdcConfig config;
    glKosInitConfig(&config);

    config.internal_palette_format = GL_RGBA8;

    glKosInitEx(&config);

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        DrawGLScene();
    }

    return 0;
}
