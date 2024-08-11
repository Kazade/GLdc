#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef __DREAMCAST__
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glext.h"
#include "GL/glkos.h"

/* using 4bpp textures from BMP files instead of 8bpp from PCX files */
#define USE_16C_PALETTE

#ifdef __DREAMCAST__
    #include <kos.h>
    extern uint8 romdisk[];
    KOS_INIT_ROMDISK(romdisk);

    #ifdef USE_16C_PALETTE
        #define IMG_PATH       "/rd/NeHe.bmp"
        #define IMG_ALPHA_PATH "/rd/NeHe-Alpha.bmp"
    #else
        #define IMG_PATH       "/rd/NeHe.pcx"
        #define IMG_ALPHA_PATH "/rd/NeHe-Alpha.pcx"
    #endif
#else
    #ifdef USE_16C_PALETTE
        #define IMG_PATH       "../samples/paletted_pcx/romdisk/NeHe.bmp"
        #define IMG_ALPHA_PATH "../samples/paletted_pcx/romdisk/NeHe-Alpha.bmp"
    #else
        #define IMG_PATH       "../samples/paletted_pcx/romdisk/NeHe.pcx"
        #define IMG_ALPHA_PATH "../samples/paletted_pcx/romdisk/NeHe-Alpha.pcx"
    #endif
#endif

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

GLuint textures[3];

typedef struct {
	uint32_t height;
	uint32_t width;
	uint32_t palette_width;
    char* palette;
    char* data;
} Image;

#ifndef  USE_16C_PALETTE

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

#else

#define BMP_BI_RGB			0L
#define BMP_BI_UNCOMPRESSED	0L
#define BMP_BI_RLE8			1L
#define BMP_BI_RLE4			2L
#define BMP_BI_BITFIELDS	3L

#pragma pack(push)
#pragma pack(1)

typedef struct BITMAP_FILE_HEADER
{
	uint16_t	Type;
	uint32_t	Size;
	uint16_t	Reserved1;
	uint16_t	Reserved2;
	uint32_t	OffBits;
} BITMAP_FILE_HEADER;

typedef struct BITMAP_INFO_HEADER
{
	uint32_t	Size;
	int32_t		Width;
	int32_t		Height;
	uint16_t	Planes;
	uint16_t	BitCount;
	uint32_t	Compression;
	uint32_t	SizeImage;
	int32_t		XPelsPerMeter;
	int32_t		YPelsPerMeter;
	uint32_t	ClrUsed;
	uint32_t	ClrImportant;
} BITMAP_INFO_HEADER;

typedef struct RGB_QUAD
{
	uint8_t	Blue;
	uint8_t	Green;
	uint8_t	Red;
	uint8_t	Reserved;
} RGB_QUAD;

typedef struct BITMAP_INFO
{
	BITMAP_INFO_HEADER	Header;
	RGB_QUAD			Colors[1];
} BITMAP_INFO;

#pragma pack(pop)

/* some global variables used to load a 4bpp BMP file */
static BITMAP_FILE_HEADER	BmpFileHeader;
static BITMAP_INFO_HEADER	BmpInfoHeader;
static RGB_QUAD			BmpRgbQuad[256];
static uint8_t				BmpPal[256 * 3];

int BMP_Infos(FILE *pFile, uint32_t *width, uint32_t *height)
{
	if (!pFile)
		return 0;

	if (fread(&BmpFileHeader.Type, 1, 2, pFile) != 2)
		return 0;
	if (fread(&BmpFileHeader.Size, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpFileHeader.Reserved1, 1, 2, pFile) != 2)
		return 0;
	if (fread(&BmpFileHeader.Reserved2, 1, 2, pFile) != 2)
		return 0;
	if (fread(&BmpFileHeader.OffBits, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.Size, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.Width, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.Height, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.Planes, 1, 2, pFile) != 2)
		return 0;
	if (fread(&BmpInfoHeader.BitCount, 1, 2, pFile) != 2)
		return 0;
	if (fread(&BmpInfoHeader.Compression, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.SizeImage, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.XPelsPerMeter, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.YPelsPerMeter, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.ClrUsed, 1, 4, pFile) != 4)
		return 0;
	if (fread(&BmpInfoHeader.ClrImportant, 1, 4, pFile) != 4)
		return 0;

	*width = (uint32_t)BmpInfoHeader.Width;
	*height = (uint32_t)BmpInfoHeader.Height;

    fseek(pFile, BmpInfoHeader.Size + 14, SEEK_SET);

	return 1;
}


int BMP_GetPalette(FILE *pFile)
{
	int32_t	i,bitCount;

	if (BmpInfoHeader.BitCount == 4) {

		if (!BmpInfoHeader.ClrImportant) {
			BmpInfoHeader.ClrImportant = 16;
		}
		bitCount = BmpInfoHeader.ClrImportant * sizeof(RGB_QUAD);

		if (fread(BmpRgbQuad, 1, bitCount, pFile) != bitCount){
            fprintf(stderr, "Failed to read palette: %ld\n", bitCount);
			return 0;
		}

		for (i = 0; i < BmpInfoHeader.ClrImportant; i++) {

			BmpPal[i * 3] = (uint8_t)BmpRgbQuad[i].Red;
			BmpPal[i * 3 + 1] = (uint8_t)BmpRgbQuad[i].Green;
			BmpPal[i * 3 + 2] = (uint8_t)BmpRgbQuad[i].Blue;
		}
		return 1;
	}

    fprintf(stderr, "BitCount: %d\n", BmpInfoHeader.BitCount);
	return 0;
}

/* maybe not the best BMP loader... */
int BMP_Depack(FILE *pFile,char *pZone)
{
	char	PadRead[4];
	int32_t	i, j, Offset, PadSize, c;

	if (BmpInfoHeader.Compression != BMP_BI_RGB)
		return 0;

	PadSize = (BmpInfoHeader.Width & 3);

	PadSize = (4 - (BmpInfoHeader.Width & 3)) & 3;

	for (i = BmpInfoHeader.Height - 1; (i > -1); i--) {
		Offset = i * BmpInfoHeader.Width / 2;

		if (PadSize < 4) {
			for (j = 0; (j < BmpInfoHeader.Width / 2); j++) {
				if (!fread(&c, 1, 1, pFile)) {
					return 0;
				}
				pZone[Offset + j] = c;
			}
		}

		if (PadSize) {
			if (fread(PadRead, PadSize, 1, pFile) != PadSize) {
				return 0;
			}
		}
	}

	if (i != -1) {
		return 0;
	}

	return 1;
}


int LoadPalettedBMP(const char* filename, Image* image)
{
	FILE *fp;
	uint32_t bytes;

	if (filename == NULL || image == NULL) {
		printf("Invalid NULL argument\n");
		return 0;
	}

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Unable to open file\n");
		return 0;
	}

	if (!BMP_Infos(fp, &image->width, &image->height)) {
		printf("Error reading BMP:%s header\n",filename);
		return 0;
	}

	if (!BMP_GetPalette(fp)) {
        printf("Only 16c BMP are supported for this sample\n");
		return 0;
	}

	/* store palette information */
	image->palette = (char*)BmpPal;
	image->palette_width = 16;


	bytes = sizeof(char) * image->width * image->height;
	/* 4bpp is half byte size*/
	bytes >>= 1;

	image->data = (char*)malloc(bytes);
	if (image->data == NULL) {
		printf("Error allocating image data");
		return 0;
	}

	if (!BMP_Depack(fp, image->data)) {
		printf("Error depacking BMP:%s",filename);
		return 0;
	}

	fclose(fp);

	return 1;
}


#endif

// Load Bitmaps And Convert To Textures
void LoadGLTextures() {
    // Load Texture
    Image image1, image2;

#ifndef USE_16C_PALETTE
    if(!LoadPalettedPCX(IMG_PATH, &image1)) {
        exit(1);
    }

    if(!LoadPalettedPCX(IMG_ALPHA_PATH, &image2)) {
        exit(1);
    }
#else
    if (!LoadPalettedBMP(IMG_PATH, &image1)) {
	exit(1);
    }

    if (!LoadPalettedBMP(IMG_ALPHA_PATH, &image2)) {
	exit(1);
    }
#endif

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
#ifndef USE_16C_PALETTE
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image1.data);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX4_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, image1.data);
#endif

    glBindTexture(GL_TEXTURE_2D, textures[1]);   // 2d texture (x and y size)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR); // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR); // scale linearly when image smalled than texture

    /* Texture-specific palette! */
    glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, image1.palette_width, GL_RGB, GL_UNSIGNED_BYTE, inversed_palette);

    // 2d texture, level of detail 0 (normal), 3 components (red, green, blue), x size from image, y size from image,
    // border 0 (normal), rgb color data, unsigned byte data, and finally the data itself.
#ifndef USE_16C_PALETTE
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image1.data);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX4_EXT, image1.width, image1.height, 0, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, image1.data);
#endif

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
#ifndef USE_16C_PALETTE
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, image2.width, image2.height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image2.data);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX4_EXT, image2.width, image2.height, 0, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, image2.data);
#endif
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
        if(check_start())
            break;

        DrawGLScene();
    }

    return 0;
}
