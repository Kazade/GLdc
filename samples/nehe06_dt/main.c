#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glkos.h"
#include "GL/glext.h"

#ifdef _arch_dreamcast
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
// /opt/toolchains/dc/kos/utils/pvrtex/pvrtex -i romdisk/NeHe.bmp -f rgb565 -c -o romdisk/NeHe.dt
#define IMG_PATH "/rd/NeHe.dt"
#else
#define IMG_PATH "../samples/nehe06_dt/romdisk/NeHe.dt"
#endif

/* floats for x rotation, y rotation, z rotation */
float xrot, yrot, zrot;

/* storage for one texture  */
GLuint texture[1];

typedef struct __attribute__((packed)) DcTxHeader {
    char fourcc[4];
    uint32_t chunk_size;
    uint8_t version;
    uint8_t header_size;
    uint8_t codebook_size;
    uint8_t colors_used;
    uint16_t width_pixels;
    uint16_t height_pixels;
    uint32_t pvr_type;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
} DcTxHeader;

typedef struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    void *buffer;
    void *data;
    GLenum format;
    GLenum internal_format;
    GLenum type;
    GLboolean compressed;
    unsigned int dataSize;
} Image;

static int ImageLoad(const char *filename, Image *image) {
    FILE *file = NULL;
    long file_size;
    DcTxHeader *header;
    size_t header_size;

    file = fopen(filename, "rb");
    if(!file) {
        printf("File Not Found : %s\n", filename);
        return 0;
    }

    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    file_size = ftell(file);
    if(file_size <= 0) {
        fclose(file);
        return 0;
    }

    rewind(file);

    image->buffer = malloc((size_t)file_size);
    if(!image->buffer) {
        fclose(file);
        return 0;
    }

    if(fread(image->buffer, (size_t)file_size, 1, file) != 1) {
        fclose(file);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    fclose(file);

    if((size_t)file_size < sizeof(DcTxHeader)) {
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    header = (DcTxHeader *)image->buffer;
    if(memcmp(header->fourcc, "DcTx", 4) != 0) {
        printf("Invalid .dt file header: %s\n", filename);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    header_size = (size_t)(header->header_size + 1) * 32;
    if(header_size > (size_t)file_size || header->chunk_size > (uint32_t)file_size) {
        printf("Invalid .dt file size: %s\n", filename);
        free(image->buffer);
        image->buffer = NULL;
        return 0;
    }

    image->sizeX = header->width_pixels;
    image->sizeY = header->height_pixels;
    image->data = (void *)((uint8_t *)image->buffer + header_size);
    image->dataSize = (unsigned int)(header->chunk_size - header_size);
    image->compressed = (header->pvr_type & (1u << 30)) != 0;

    {
        GLboolean twiddled = ((header->pvr_type & (1u << 26)) == 0);
        GLboolean mipmapped = ((int32_t)header->pvr_type) < 0;
        GLuint format = (header->pvr_type >> 27) & 0x7;

        if(image->compressed) {
            switch(format) {
                case 0:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS : GL_COMPRESSED_ARGB_1555_VQ_KOS);
                    break;
                case 1:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_RGB_565_VQ_TWID_KOS : GL_COMPRESSED_RGB_565_VQ_KOS);
                    break;
                case 2:
                    image->internal_format = mipmapped
                        ? (twiddled ? GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS : GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS)
                        : (twiddled ? GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS : GL_COMPRESSED_ARGB_4444_VQ_KOS);
                    break;
                default:
                    printf("Unsupported compressed texture format in .dt: %u\n", format);
                    free(image->buffer);
                    image->buffer = NULL;
                    return 0;
            }
        } else {
            switch(format) {
                case 0:
                    image->format = GL_BGRA;
                    image->internal_format = GL_RGBA;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS : GL_UNSIGNED_SHORT_1_5_5_5_REV;
                    break;
                case 1:
                    image->format = GL_RGB;
                    image->internal_format = GL_RGB;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_5_6_5_TWID_KOS : GL_UNSIGNED_SHORT_5_6_5;
                    break;
                case 2:
                    image->format = GL_BGRA;
                    image->internal_format = GL_RGBA;
                    image->type = twiddled ? GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS : GL_UNSIGNED_SHORT_4_4_4_4_REV;
                    break;
                default:
                    printf("Unsupported uncompressed texture format in .dt: %u\n", format);
                    free(image->buffer);
                    image->buffer = NULL;
                    return 0;
            }
        }
    }

    return 1;
}

/* Load texture from a .dt file */
static void LoadGLTextures(void) {
    Image *image1;

    image1 = (Image *)malloc(sizeof(Image));
    if(image1 == NULL) {
        printf("Error allocating space for image\n");
        exit(0);
    }

    memset(image1, 0, sizeof(*image1));

    if(!ImageLoad(IMG_PATH, image1)) {
        free(image1);
        exit(1);
    }

    glGenTextures(1, &texture[0]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if(image1->compressed) {
        glCompressedTexImage2DARB(
            GL_TEXTURE_2D, 0, image1->internal_format, image1->sizeX, image1->sizeY, 0,
            image1->dataSize, image1->data
        );
    } else {
        glTexImage2D(
            GL_TEXTURE_2D, 0, image1->internal_format, image1->sizeX, image1->sizeY, 0,
            image1->format, image1->type, image1->data
        );
    }

    free(image1->buffer);
    free(image1);
}

/* A general OpenGL initialization function. */
static void InitGL(int Width, int Height) {
    LoadGLTextures();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
}

/* The function called when our window is resized. */
static void ReSizeGLScene(int Width, int Height) {
    if(Height == 0)
        Height = 1;

    glViewport(0, 0, Width, Height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f, (GLfloat)Width / (GLfloat)Height, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

static int check_start(void) {
#ifdef _arch_dreamcast
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
static void DrawGLScene(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -5.0f);

    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);
    glRotatef(zrot, 0.0f, 0.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, texture[0]);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);

    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);

    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);

    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);

    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);

    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);

    glEnd();

    xrot += 1.5f;
    yrot += 1.5f;
    zrot += 1.5f;

    glKosSwapBuffers();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

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
