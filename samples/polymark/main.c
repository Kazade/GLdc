/*
   KallistiGL 2.0.0

   quadmark.c
   (c)2018 Luke Benstead
   (c)2014 Josh Pearson
   (c)2002 Dan Potter, Paul Boese
*/

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#include <GL/gl.h>
#include <GL/glkos.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { PHASE_HALVE, PHASE_INCR, PHASE_DECR, PHASE_FINAL };

int polycnt;
int phase = PHASE_HALVE;
float avgfps = -1;

void running_stats() {
#ifdef _arch_dreamcast
    pvr_stats_t stats;
    pvr_get_stats(&stats);

    if(avgfps == -1)
        avgfps = stats.frame_rate;
    else
        avgfps = (avgfps + stats.frame_rate) / 2.0f;
#endif
}

void stats() {
#ifdef _arch_dreamcast
    pvr_stats_t stats;

    pvr_get_stats(&stats);
    dbglog(DBG_DEBUG, "3D Stats: %d VBLs, frame rate ~%f fps\n",
           stats.vbl_count, stats.frame_rate);
#endif
}


int check_start() {
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

void setup() {
    glKosInit();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -100, 100);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glEnable(GL_CULL_FACE);
}

void do_frame() {
    int x, y, z;
    int size;
    int i;
    float col;

    for(i = 0; i < polycnt; i++) {
        glBegin(GL_POLYGON);
        x = rand() % 640;
        y = rand() % 480;
        z = rand() % 100 + 1;
        size = rand() % 50 + 1;
        col = (rand() % 255) * 0.00391f;

        glColor3f(col, col, col);
        glVertex3f(x - size, y - size, z);
        glVertex3f(x + size, y - size, z);
        glVertex3f(x + size, y + size, z);
        glVertex3f(x, y + size + (size / 2), z);
        glVertex3f(x - size, y + size, z);
        glEnd();
    }

    glKosSwapBuffers();
}

time_t begin;
void switch_tests(int ppf) {
    printf("Beginning new test: %d polys per frame (%d per second at 60fps)\n",
           ppf * 3, ppf * 3 * 60);
    avgfps = -1;
    polycnt = ppf;
}

void check_switch() {
    time_t now;

    now = time(NULL);

    if(now >= (begin + 5)) {
        begin = time(NULL);
        printf("  Average Frame Rate: ~%f fps (%d pps)\n", avgfps, (int)(polycnt * avgfps * 2));

        switch(phase) {
            case PHASE_HALVE:

                if(avgfps < 55) {
                    switch_tests(polycnt / 1.2f);
                }
                else {
                    printf("  Entering PHASE_INCR\n");
                    phase = PHASE_INCR;
                }

                break;

            case PHASE_INCR:

                if(avgfps >= 55) {
                    switch_tests(polycnt + 15);
                }
                else {
                    printf("  Entering PHASE_DECR\n");
                    phase = PHASE_DECR;
                }

                break;

            case PHASE_DECR:

                if(avgfps < 55) {
                    switch_tests(polycnt - 30);
                }
                else {
                    printf("  Entering PHASE_FINAL\n");
                    phase = PHASE_FINAL;
                }

                break;

            case PHASE_FINAL:
                break;
        }
    }
}

int main(int argc, char **argv) {
    setup();

    /* Start off with something obscene */
    switch_tests(200000 / 60);
    begin = time(NULL);

    for(;;) {
        if(check_start())
            break;

        printf(" \r");
        do_frame();
        running_stats();
        check_switch();
    }

    stats();

    return 0;
}


