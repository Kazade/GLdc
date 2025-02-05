#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <GL/gl.h>
#include <GL/glkos.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include "gl_png.h"

#define CLEANUP(x) { ret = (x); goto cleanup; }

GLfloat global_diffuse[] = {1.0, 1.0, 1.0, 1.0};
GLfloat global_ambient[] = {1.0, 1.0, 1.0, 1.0};

static int decode_dtex(Image* image, FILE* file) {
	struct {
			char	id[4];	// 'DTEX'
			GLushort	width;
			GLushort	height;
			GLuint		type;
			GLuint		size;
	} header;

	fread(&header, sizeof(header), 1, file);

	image->twiddled   = (header.type & (1 << 26)) < 1;
	image->compressed = (header.type & (1 << 30)) > 0;
	image->mipmapped  = (header.type & (1 << 31)) > 0;
	GLuint format = (header.type >> 27) & 0b111;

	image->data = (char *) malloc (header.size);
	image->sizeX = header.width;
	image->sizeY = header.height;
	image->dataSize = header.size;

	fread(image->data, image->dataSize, 1, file);
	fclose(file);

	if(image->compressed && image->twiddled) {
		switch(format) {
			case 0:
				puts("Compressed & Twiddled - ARGB 1555");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS;
				}
				return 1;
			
			case 1:
				puts("Compressed & Twiddled - RGB 565");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_RGB_565_VQ_TWID_KOS;
				}
				return 1;
			
			case 2:
				puts("Compressed & Twiddled - ARGB 4444");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS;
				}
				return 1;
			
		}
	} else if (image->compressed) {
		switch(format) {
			case 0:
				puts("Compressed - ARGB 1555");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_KOS;
				}
				return 1;
				
			case 1:
				puts("Compressed - RGB 565");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_RGB_565_VQ_KOS;
				}
				return 1;
				
			case 2:
				puts("Compressed - ARGB 4444");
				if(image->mipmapped) {
					image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS;
				} else {
					image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_KOS;
				}
				return 1;
		}
	} else {
		switch (format) {
			case 0:
				puts("Uncompressed - ARGB 1555");
				image->internalFormat = GL_ARGB1555_TWID_KOS;
				image->transferFormat = GL_BGRA;
				image->transferType   = GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS;
				return 1;
				
			case 1:
				puts("Uncompressed - RGB 565");
				image->internalFormat = GL_RGB565_TWID_KOS;
				image->transferFormat = GL_RGB;
				image->transferType   = GL_UNSIGNED_SHORT_5_6_5_TWID_KOS;
				return 1;
				
			case 2:
				puts("Uncompressed - ARGB 4444");
				image->internalFormat = GL_ARGB4444_TWID_KOS;
				image->transferFormat = GL_BGRA;
				image->transferType   = GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS;
				return 1;
		}
	}
	
	fprintf(stderr, "Invalid texture format %u\n", header.type);
	return 0;
}

int dtex_to_gl_texture(texture *tex, char* filename) {
    // Load Texture
    Image image = { 0 };

	FILE* file = NULL;

	// make sure the file is there.
	if ((file = fopen(filename, "rb")) == NULL)
	{
		printf("File not found");
			return 0;
	}

	decode_dtex(&image, file);

	// Create Texture
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);   // 2d texture (x and y size)

	if (image.compressed) {
		glCompressedTexImage2D(GL_TEXTURE_2D, 0,
			image.internalFormat, image.sizeX, image.sizeY,
			0, image.dataSize, image.data);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0,
			image.internalFormat, image.sizeX, image.sizeY, 0,
			image.transferFormat, image.transferType, image.data);
	}

	tex->id = texture_id;
	tex->w = image.sizeX;
	tex->h = image.sizeY;
	tex->u = 0.f;
	tex->v = 0.f;
	tex->a = tex->light = 1;
	tex->color[0] = tex->color[1] = tex->color[2] = 1.0f;
	tex->uSize = tex->vSize = 1.0f;
	tex->xScale = tex->yScale = 1.0f;
	tex->format = image.internalFormat;
	tex->min_filter = tex->mag_filter = GL_NEAREST;
	tex->blend_source = GL_SRC_ALPHA;
	tex->blend_dest = GL_ONE_MINUS_SRC_ALPHA;
	strcpy(tex->path, filename);
	
	GLuint expected = 2 * image.sizeX * image.sizeY;
	GLuint ratio = (GLuint) (((GLfloat) expected) / ((GLfloat) image.dataSize));

	printf("Texture size: %lu x %lu\n", image.sizeX, image.sizeY);
	printf("Texture ratio: %d\n", ratio);
	printf("Texture size: %lu x %lu\n", image.sizeX, image.sizeY);
	printf("Texture %s loaded\n", tex->path);

	return(1);
}

void draw_textured_quad(texture *tex) {
	if(glIsTexture(tex->id)) {

		GLfloat vertex_data[] = {
			/* 2D Coordinate, texture coordinate */
			0, 1, 0,
			1, 1, 0,
			1, 0, 0,
			0, 0, 0
		};

		GLfloat uv_data[] = {
			/* 2D Coordinate, texture coordinate */
			0, 1,
			1, 1,
			1, 0,
			0, 0
		};

		GLfloat normal_data[] = {
			/* 2D Coordinate, texture coordinate */
			0.0, 0.0, 1.0,
			0.0, 0.0, 1.0,
			0.0, 0.0, 1.0,
			0.0, 0.0, 1.0
		};

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, tex->id);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);

		glVertexPointer			(3, GL_FLOAT, 0, vertex_data);
		glTexCoordPointer		(2, GL_FLOAT, 0, uv_data);
		glNormalPointer			(GL_FLOAT, 0, normal_data);

		glDrawArrays(GL_QUADS, 0, 4);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisable(GL_TEXTURE_2D);
	}
}
