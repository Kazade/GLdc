#include <stdio.h>

#include "gl.h"
#include "glu.h"
#include "glkos.h"

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);


float xrot, yrot, zrot;


int texture[1];


struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    char *data;
};
typedef struct Image Image;



int ImageLoad(char *filename, Image *image) {
    FILE *file;
    unsigned long size;
    unsigned long i;
    unsigned short int planes;
    unsigned short int bpp;
    char temp;


    if ((file = fopen(filename, "rb"))==NULL)
    {
        printf("File Not Found : %s\n",filename);
        return 0;
    }


    fseek(file, 18, SEEK_CUR);


    if ((i = fread(&image->sizeX, 4, 1, file)) != 1) {
        printf("Error reading width from %s.\n", filename);
        return 0;
    }
    printf("Width of %s: %lu\n", filename, image->sizeX);


    if ((i = fread(&image->sizeY, 4, 1, file)) != 1) {
        printf("Error reading height from %s.\n", filename);
        return 0;
    }
    printf("Height of %s: %lu\n", filename, image->sizeY);


    size = image->sizeX * image->sizeY * 3;


    if ((fread(&planes, 2, 1, file)) != 1) {
        printf("Error reading planes from %s.\n", filename);
        return 0;
    }
    if (planes != 1) {
        printf("Planes from %s is not 1: %u\n", filename, planes);
        return 0;
    }


    if ((i = fread(&bpp, 2, 1, file)) != 1) {
        printf("Error reading bpp from %s.\n", filename);
        return 0;
    }
    if (bpp != 24) {
        printf("Bpp from %s is not 24: %u\n", filename, bpp);
        return 0;
    }


    fseek(file, 24, SEEK_CUR);


    image->data = (char *) malloc(size);
    if (image->data == NULL) {
        printf("Error allocating memory for color-corrected image data");
        return 0;
    }

    if ((i = fread(image->data, size, 1, file)) != 1) {
        printf(stderr, "Error reading image data from %s.\n", filename);
        return 0;
    }

    for (i=0;i<size;i+=3) {
        temp = image->data[i];
        image->data[i] = image->data[i+2];
        image->data[i+2] = temp;
    }


    return 1;
}


void LoadGLTextures() {

    Image *image1;


    image1 = (Image *) malloc(sizeof(Image));
    if (image1 == NULL) {
        printf("Error allocating space for image");
        exit(0);
    }

    if (!ImageLoad("/rd/NeHe.bmp", image1)) {
        exit(1);
    }

    glGenTextures(1, &texture[0]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);



    glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->sizeX, image1->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image1->data);

    glGenerateMipmapEXT(GL_TEXTURE_2D);
};


void InitGL(int Width, int Height)
{
    LoadGLTextures();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glClearColor(0.5, 0.5, 0.5, 0.5);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

        // Create light components
    GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuseLight[] = { 1.0f, 0.0f, 0.0, 1.0f };
    GLfloat specularLight[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    GLfloat position[] = { -1.5f, -1.0f, 0.0f, 0.0f };

    // Assign created components to GL_LIGHT0
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    glLightfv(GL_LIGHT0, GL_POSITION, position);

    glDisable(GL_TEXTURE_2D);

    diffuseLight[1] = 1.0f;

    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT1, GL_SPECULAR, specularLight);

    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 4.5 / 100);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 75.0f / (100 * 100));

    glEnable(GL_NORMALIZE);
}


void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0)
        Height = 1;

    glViewport(0, 0, Width, Height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);
    glMatrixMode(GL_MODELVIEW);
}

void DrawCube(float x, float z) {
    static float pos = 0.0f;
    const static float radius = 30.0f;

    pos += 0.001f;

    GLfloat position[] = { cos(pos) * radius, 15.0f, sin(pos) * radius, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, position);

    glPushMatrix();
    glTranslatef(x, 0, z);
    glColor4f(1, 1, 1, 1);

    glBegin(GL_QUADS);


    glNormal3f(0, 0, -1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);


    glNormal3f(0, 0, 1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);


    glNormal3f(0, 1, 0);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);

    glNormal3f(0, -1, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);

    glNormal3f(1, 0, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);

    glNormal3f(-1, 0, 0);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);

    glEnd();

    glPopMatrix();
}

void DrawGLScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();


    glTranslatef(0.0f, 0.0f, -55.0f);
    glRotatef(45, 1, 0, 0);
    glTranslatef(0, 0, -30);

    glBindTexture(GL_TEXTURE_2D, texture[0]);

    int x, z;
    for(z = -100; z < 100; z += 10) {
        for(x = -100; x < 100; x += 10) {
            DrawCube(x, z);
        }
    }

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
