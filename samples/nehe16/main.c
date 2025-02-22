/* 
   KallistiOS 2.0.0

   nehe16.c
   (c)2025 Falco Girgis
   (c)2014 Josh Pearson
   (c)2001 Benoit Miller
   (c)2000 Jeff Molofee
*/

#include <stdio.h>
#include <GL/glkos.h>
#include <GL/gl.h>
#include <GL/glu.h>

#ifdef __DREAMCAST__
#include <kos.h>
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#define IMG_PATH "/rd/glass.pvr"
#else
#define IMG_PATH "../samples/nehe08/romdisk/glass.pvr"
#endif

/* Simple GL example to demonstrate fog (PVR table fog).

   Essentially the same thing as NeHe's lesson16 code.
   To learn more, go to http://nehe.gamedev.net/.

   DPAD controls the cube rotation, button A & B control the depth
   of the cube, button X toggles fog on/off, and button Y toggles fog type.
*/

static GLfloat xrot;        /* X Rotation */
static GLfloat yrot;        /* Y Rotation */
static GLfloat xspeed;      /* X Rotation Speed */
static GLfloat yspeed;      /* Y Rotation Speed */
static GLfloat z = -5.0f;   /* Depth Into The Screen */

static GLuint  texture;         /* Storage For Texture */

/* Storage For Three Types Of Fog */
GLuint fogType = 0; /* use GL_EXP initially */
GLuint fogMode[] = { GL_EXP, GL_EXP2, GL_LINEAR };
char cfogMode[3][10] = {"GL_EXP   ", "GL_EXP2  ", "GL_LINEAR" };
GLfloat fogColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; /* Fog Color */
int fog = GL_TRUE;

/* Load a PVR texture - located in pvr-texture.c */
extern GLuint glTextureLoadPVR(char *fname, unsigned char isMipMapped, unsigned char glMipMap);

void draw_gl(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, z);

    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_QUADS);
    /* Front Face */
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    /* Back Face */
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    /* Top Face */
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f,  1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f,  1.0f, -1.0f);
    /* Bottom Face */
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    /* Right face */
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f,  1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f,  1.0f);
    /* Left Face */
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glEnd();

    xrot += xspeed;
    yrot += yspeed;
}

int main(int argc, char **argv) {
    GLboolean xp = GL_FALSE;
    GLboolean yp = GL_FALSE;

    printf("nehe16 beginning\n");

    /* Get basic stuff initialized */
    glKosInit();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glColor4f(1.0f, 1.0f, 1.0f, 0.5);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    /* Enable Lighting and GL_LIGHT0 */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    /* Set up the fog */
    glFogi(GL_FOG_MODE, fogMode[fogType]);     /* Fog Mode */
    glFogfv(GL_FOG_COLOR, fogColor);           /* Set Fog Color */
    glFogf(GL_FOG_DENSITY, 0.35f);             /* How Dense The Fog is */
    glHint(GL_FOG_HINT, GL_DONT_CARE);         /* Fog Hint Value */
    glFogf(GL_FOG_START, 0.0f);                /* Fog Start Depth */
    glFogf(GL_FOG_END, 5.0f);                  /* Fog End Depth */
    glEnable(GL_FOG);                          /* Enables GL_FOG */

    /* Set up the textures */
    texture = glTextureLoadPVR(IMG_PATH, 0, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    while(1) {
#ifdef __DREAMCAST__
        maple_device_t *cont;
        cont_state_t *state;

        cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

        /* Check key status */
        state = (cont_state_t *)maple_dev_status(cont);

        if(!state) {
            printf("Error reading controller\n");
            break;
        }

        if(state->start)
            break;

        if(state->a) {
            if(z >= -15.0f) z -= 0.02f;
        }

        if(state->b) {
            if(z <= 0.0f) z += 0.02f;
        }

        if(state->x && !xp) {
            xp = GL_TRUE;
            fogType = (fogType + 1) % 3;
            glFogi(GL_FOG_MODE, fogMode[fogType]);
            printf("%s\n", cfogMode[fogType]);
        }

        if(!state->x)
            xp = GL_FALSE;

        if(state->y && !yp) {
            yp = GL_TRUE;
            fog = !fog;
        }

        if(!state->y)
            yp = GL_FALSE;

        if(state->dpad_up)
            xspeed -= 0.1f;

        if(state->dpad_down)
            xspeed += 0.1f;

        if(state->dpad_left)
            yspeed -= 0.1f;

        if(state->dpad_right)
            yspeed += 0.1f;
#endif

        /* Switch fog off/on */
        if(fog) {
            glEnable(GL_FOG);
        }
        else {
            glDisable(GL_FOG);
        }

        /* Draw the GL "scene" */
        draw_gl();

        /* Finish the frame */
        glKosSwapBuffers();
    }

    return 0;
}


