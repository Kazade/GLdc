#ifndef __GL_PNG_H__
#define __GL_PNG_H__

#include <stdio.h>
#include <kos.h>
#include <GL/gl.h>

typedef struct _texture {
	GLuint 		id;
	GLenum 		format;
	GLenum 		min_filter;
	GLenum 		mag_filter;
	GLenum		blend_source;
	GLenum		blend_dest;
	int 			loaded;
	uint16_t 	w, h; // width / height of texture image
	int				size[2];
	float 		u, v; //uv COORD
	float 		uSize, vSize; // uvSize
	float 		xScale, yScale; //render scale
	float 		a; //alpha
	float 		light; //alpha
	float 		color[3];
	char  		path[32];
} texture;

/* DTEX Image type - contains height, width, and data */
typedef struct Image {
    unsigned long sizeX;
    unsigned long sizeY;
    char 					*data;
    GLenum 				internalFormat;
    GLboolean 		mipmapped;
    unsigned int 	dataSize;
} Image;


int 	dtex_to_gl_texture(texture *tex, char* filename);
void 	draw_textured_quad(texture *tex);


#endif
