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


int dtex_to_gl_texture(texture *tex, char* filename) {
    // Load Texture
    Image *image;

    // allocate space for texture
    image = (Image *) malloc(sizeof(Image));
    if (image == NULL) {
				printf("No memory for .DTEX file\n");
				return(0);
    }

		FILE* file = NULL;

		// make sure the file is there.
		if ((file = fopen(filename, "rb")) == NULL)
		{
			printf("File not found");
				return 0;
		}

		struct {
				char	id[4];	// 'DTEX'
				GLushort	width;
				GLushort	height;
				GLuint		type;
				GLuint		size;
		} header;

		fread(&header, sizeof(header), 1, file);

		GLboolean twiddled = (header.type & (1 << 26)) < 1;
		GLboolean compressed = (header.type & (1 << 30)) > 0;
		GLboolean mipmapped = (header.type & (1 << 31)) > 0;
		GLuint 		format = (header.type >> 27) & 0b111;

		image->data = (char *) malloc (header.size);
		image->sizeX = header.width;
		image->sizeY = header.height;
		image->dataSize = header.size;

		GLuint expected = 2 * header.width * header.height;
		GLuint ratio = (GLuint) (((GLfloat) expected) / ((GLfloat) header.size));

		fread(image->data, image->dataSize, 1, file);
		fclose(file);

		if(compressed) {
				printf("Compressed - ");
				if(twiddled) {
					printf("Twiddled - ");
						switch(format) {
								case 0: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS;
										}
								} break;
								case 1: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_RGB_565_VQ_TWID_KOS;
										}
								} break;
								case 2: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS;
										}
								}
								break;
								default:
										fprintf(stderr, "Invalid texture format");
										return 0;
						}
				} else {
						switch(format) {
								case 0: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_ARGB_1555_VQ_KOS;
										}
								} break;
								case 1: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_RGB_565_VQ_KOS;
										}
								} break;
								case 2: {
										if(mipmapped) {
												image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS;
										} else {
												image->internalFormat = GL_COMPRESSED_ARGB_4444_VQ_KOS;
										}
								}
								break;
								default:
										fprintf(stderr, "Invalid texture format");
										return 0;
						}
				}
		} else {
			printf("Uncompressed - ");
			//printf("Color:%u -", format);
				switch(format) {

						case 0:
								image->internalFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS;
								//image->internalFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
						break;
						case 1:
								image->internalFormat = GL_UNSIGNED_SHORT_5_6_5_REV;
						break;
						case 2:
								image->internalFormat = GL_UNSIGNED_SHORT_4_4_4_4_REV;
						break;
			}
		}
		printf("\n");

		// Create Texture
		GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);   // 2d texture (x and y size)

		GLint newFormat = format;
		GLint colorType = GL_RGB;

		if (image->internalFormat == GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS ||
				image->internalFormat == GL_UNSIGNED_SHORT_4_4_4_4_REV){
					 newFormat = GL_BGRA;
					 colorType = GL_RGBA;
					 printf("Reversing RGBA\n");
			}

		if (image->internalFormat == GL_UNSIGNED_SHORT_5_6_5_REV){
					 newFormat = GL_RGB;
					 colorType = GL_RGB;
					 printf("Reversing RGB\n");
			}

		glTexImage2D(GL_TEXTURE_2D, 0,
			colorType, image->sizeX, image->sizeY, 0,
			newFormat, image->internalFormat, image->data);

		tex->id = texture_id;
		tex->w = image->sizeX;
		tex->h = image->sizeY;
		tex->u = 0.f;
		tex->v = 0.f;
		tex->a = tex->light = 1;
		tex->color[0] = tex->color[1] = tex->color[2] = 1.0f;
		tex->uSize = tex->vSize = 1.0f;
		tex->xScale = tex->yScale = 1.0f;
		tex->format = image->internalFormat;
		tex->min_filter = tex->mag_filter = GL_NEAREST;
		tex->blend_source = GL_SRC_ALPHA;
		tex->blend_dest = GL_ONE_MINUS_SRC_ALPHA;
		strcpy(tex->path, filename);

		printf("Texture size: %lu x %lu\n", image->sizeX, image->sizeY);
		printf("Texture ratio: %d\n", ratio);
		printf("Texture size: %lu x %lu\n", image->sizeX, image->sizeY);
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
