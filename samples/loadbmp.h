// quick and dirty bitmap loader...for 24 bit bitmaps with 1 plane only.
// See http://www.dcs.ed.ac.uk/~mxr/gfx/2d/BMP.txt for more info.
#ifndef __LOADBMP_H
#define __LOADBMP_H

/* Image type - contains height, width, and data */
struct Image {
    unsigned int sizeX;
    unsigned int sizeY;
    char *data;
};
typedef struct Image Image;

int ImageLoad(char *, Image *);

#endif
