/*
	tga.c	WJ108

	Targa image loader

	I actually used code by David Henry
	see http://tfcduke.developpez.com/tutoriel/format/tga/


	usage:
		tga_image = load_tga("img01.tga");
		... do something cool ...
		free(tga);
*/

#include "tga.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL_opengl.h"


#pragma pack(push, 1)
typedef struct {
	unsigned char id_lenght;		/* size of image id */
	unsigned char colormap_type;	/* 1 is has a colormap */
	unsigned char image_type;		/* compression type */

	short cm_first_entry;			/* colormap origin */
	short cm_length;				/* colormap length */
	unsigned char cm_size;			/* colormap size */

	short x_origin;					/* bottom left x coord origin */
	short y_origin;					/* bottom left y coord origin */

	short width;					/* picture width (in pixels) */
	short height;					/* picture height (in pixels) */

	unsigned char pixel_depth;		/* bits per pixel: 8, 16, 24 or 32 */
	unsigned char image_descriptor;	/* 24 bits = 0x00; 32 bits = 0x80 */
} TGA_Header;
#pragma pack(pop)


static void internal_format(TGA *tga, TGA_Header *header) {
	tga->w = header->width;
	tga->h = header->height;

	switch(header->image_type) {
		case 3:				/* grayscale 8 bits */
		case 11:			/* grayscale 8 bits (RLE) */
			if (header->pixel_depth == 8) {
				tga->format = GL_LUMINANCE;
				tga->bytes_per_pixel = 1;
			} else {		/* 16 bits */
				tga->format = GL_LUMINANCE_ALPHA;
				tga->bytes_per_pixel = 2;
			}
			break;

		case 1:				/* 8 bits color index */
		case 2:				/* BGR 16-24-32 bits */
		case 9:				/* 8 bits color index (RLE) */
		case 10:			/* BGR 16-24-32 bits (RLE) */
/* 8 bits and 16 bits images will be converted to 24 bits */
			if (header->pixel_depth <= 24) {
				tga->format = GL_RGB;
				tga->bytes_per_pixel = 3;
			} else {		/* 32 bits */
				tga->format = GL_RGBA;
				tga->bytes_per_pixel = 4;
			}
			break;
	}
}

static void ReadTGA8bits(FILE *f, unsigned char *colormap, TGA *tga) {
int i, pos, color;

	for(i = 0; i < tga->w * tga->h; i++) {
		color = fgetc(f) & 0xff;

/* convert to RGB 24 bits */
		color *= 3;
		pos = i * 3;
		tga->pixels[pos+2] = colormap[color];
		tga->pixels[pos+1] = colormap[color+1];
		tga->pixels[pos] = colormap[color+2];
	}
}

static void ReadTGA16bits(FILE *f, TGA *tga) {
int i, pos, color;

	for(i = 0; i < tga->w * tga->h; i++) {
		color = (fgetc(f) & 0xff) | ((fgetc(f) & 0xff) << 8);
/*
	convert BGR to RGB
	This is 5 bits per color component, highest bit is 0 (total 16 bits)
*/
		pos = i * 3;
		tga->pixels[pos] = (unsigned char)(((color & 0x7c00) >> 10) << 3);
		tga->pixels[pos+1] = (unsigned char)(((color & 0x03e0) >>  5) << 3);
		tga->pixels[pos+2] = (unsigned char)(((color & 0x001f) >>  0) << 3);
    }
}

static void ReadTGA24bits(FILE *f, TGA *tga) {
int i, pos;

	for(i = 0; i < tga->w * tga->h; i++) {
/* read and convert BGR to RGB */
		pos = i * 3;
		tga->pixels[pos+2] = (unsigned char)(fgetc(f) & 0xff);
		tga->pixels[pos+1] = (unsigned char)(fgetc(f) & 0xff);
		tga->pixels[pos] = (unsigned char)(fgetc(f) & 0xff);
	}
}

static void ReadTGA32bits(FILE *f, TGA *tga) {
int i, pos;

	for(i = 0; i < tga->w * tga->h; i++) {
/* read and convert BGRA to RGBA */
		pos = i * 4;
		tga->pixels[pos+2] = (unsigned char)fgetc(f);
		tga->pixels[pos+1] = (unsigned char)fgetc(f);
		tga->pixels[pos] = (unsigned char)fgetc(f);
		tga->pixels[pos+3] = (unsigned char)fgetc(f);
	}
}

static void ReadTGAgray8bits(FILE *f, TGA *tga) {
	fread(tga->pixels, tga->w, tga->h, f);
}

static void ReadTGAgray16bits(FILE *f, TGA *tga) {
int i, pos;

	for(i = 0; i < tga->w * tga->h; i++) {
/* read grayscale color + alpha channel bytes */
		pos = i * 2;
		tga->pixels[pos] = (unsigned char)(fgetc(f) & 0xff);
		tga->pixels[pos+1] = (unsigned char)(fgetc(f) & 0xff);
    }
}

static void ReadTGA8bitsRLE(FILE *f, unsigned char *colormap, TGA *tga) {
int i, size;
unsigned char color, packet_header, *ptr = tga->pixels;

	while(ptr < tga->pixels + (tga->w * tga->h) * 3) {
		packet_header = (unsigned char)(fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			color = (unsigned char)(fgetc(f) & 0xff);
			color *= 3;

			for (i = 0; i < size; i++) {
				ptr[0] = colormap[color+2];
				ptr[1] = colormap[color+1];
				ptr[2] = colormap[color];
				ptr += 3;
			}
		} else {								/* non run-length packet */
			for(i = 0; i < size; i++) {
				color = (unsigned char)(fgetc(f) & 0xff);
				color *= 3;

				ptr[0] = colormap[color+2];
				ptr[1] = colormap[color+1];
				ptr[2] = colormap[color];
				ptr += 3;
			}
		}
	}
}

static void ReadTGA16bitsRLE(FILE *f, TGA *tga) {
int i, size;
unsigned short color;
unsigned char packet_header;
unsigned char *ptr = tga->pixels;

	while(ptr < tga->pixels + (tga->w * tga->h) * 3) {
		packet_header = (fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			color = (fgetc(f) & 0xff) | ((fgetc(f) << 8) & 0xff00);

			for (i = 0; i < size; i++) {
				ptr[0] = (unsigned char)(((color & 0x7c00) >> 10) << 3);
				ptr[1] = (unsigned char)(((color & 0x03e0) >>  5) << 3);
				ptr[2] = (unsigned char)(((color & 0x001f) >>  0) << 3);
				ptr += 3;
			}
		} else {								/* non run-length packet */
			for (i = 0; i < size; ++i, ptr += 3) {
				color = (fgetc(f) & 0xff) | ((fgetc(f) << 8) & 0xff00);

				ptr[0] = (unsigned char)(((color & 0x7c00) >> 10) << 3);
				ptr[1] = (unsigned char)(((color & 0x03e0) >>  5) << 3);
				ptr[2] = (unsigned char)(((color & 0x001f) >>  0) << 3);
			}
		}
	}
}

static void ReadTGA24bitsRLE(FILE *f, TGA *tga) {
int i, size;
unsigned char rgb[3], packet_header, *ptr = tga->pixels;

	while (ptr < tga->pixels + (tga->w * tga->h) * 3) {
		packet_header = (unsigned char)(fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			fread(rgb, 1, 3, f);

			for(i = 0; i < size; i++) {
				ptr[0] = rgb[2];
				ptr[1] = rgb[1];
				ptr[2] = rgb[0];
				ptr += 3;
			}
		} else {								/* non run-length packet */
			for(i = 0; i < size; i++) {
				ptr[2] = (unsigned char)(fgetc(f) & 0xff);
				ptr[1] = (unsigned char)(fgetc(f) & 0xff);
				ptr[0] = (unsigned char)(fgetc(f) & 0xff);
				ptr += 3;
			}
		}
	}
}

static void ReadTGA32bitsRLE(FILE *f, TGA *tga) {
int i, size;
unsigned char rgba[4], packet_header, *ptr = tga->pixels;

	while (ptr < tga->pixels + (tga->w * tga->h) * 4) {
		packet_header = (unsigned char)(fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			fread(rgba, 1, 4, f);

			for(i = 0; i < size; i++) {
				ptr[0] = rgba[2];
				ptr[1] = rgba[1];
				ptr[2] = rgba[0];
				ptr[3] = rgba[3];
				ptr += 4;
			}
		} else {								/* non run-length packet */
			for(i = 0; i < size; i++) {
				ptr[2] = (unsigned char)(fgetc(f) & 0xff);
				ptr[1] = (unsigned char)(fgetc(f) & 0xff);
				ptr[0] = (unsigned char)(fgetc(f) & 0xff);
				ptr[3] = (unsigned char)(fgetc(f) & 0xff);
				ptr += 4;
			}
		}
	}
}

static void ReadTGAgray8bitsRLE(FILE *f, TGA *tga) {
int i, size;
unsigned char color, packet_header, *ptr = tga->pixels;

	while (ptr < tga->pixels + (tga->w * tga->h)) {
		packet_header = (unsigned char)(fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			color = (unsigned char)(fgetc(f) & 0xff);

			for(i = 0; i < size; i++) {
				*ptr = color;
				ptr++;
			}
		} else {								/* non run-length packet */
			for(i = 0; i < size; i++) {
				*ptr = (unsigned char)(fgetc(f) & 0xff);
				ptr++;
			}
		}
	}
}

static void ReadTGAgray16bitsRLE(FILE *f, TGA *tga) {
int i, size;
unsigned char color, alpha, packet_header, *ptr = tga->pixels;

	while (ptr < tga->pixels + (tga->w * tga->h) * 2) {
		packet_header = (unsigned char)(fgetc(f) & 0xff);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80) {				/* run-length packet */
			color = (unsigned char)(fgetc(f) & 0xff);
			alpha = (unsigned char)(fgetc(f) & 0xff);

			for (i = 0; i < size; i++) {
				ptr[0] = color;
				ptr[1] = alpha;
				ptr += 2;
			}
		} else {								/* non run-length packet */
			for (i = 0; i < size; i++) {
				ptr[0] = (unsigned char)(fgetc(f) & 0xff);
				ptr[1] = (unsigned char)(fgetc(f) & 0xff);
				ptr += 2;
			}
		}
	}
}


TGA *load_tga(char *filename) {
TGA *tga;
TGA_Header header;
unsigned char palette[256 * 3];
FILE *f;

	if (filename == NULL || !*filename)
		return NULL;

	if ((f = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "load_tga(): failed to open file '%s'\n", filename);
		return NULL;
	}
/* read header */
	fread(&header, sizeof(TGA_Header), 1, f);

	if (feof(f)) {
		fprintf(stderr, "load_tga(): failed to load '%s'\n", filename);
		fclose(f);
		return NULL;
	}
	if (fseek(f, header.id_lenght, SEEK_CUR) == -1) {
		fprintf(stderr, "load_tga(): failed to load '%s'\n", filename);
		fclose(f);
		return NULL;
	}
/* allocate pixel data right after the struct TGA */
	if ((tga = (TGA *)malloc(sizeof(TGA) + header.width * header.height * header.pixel_depth / 8)) == NULL) {
		fprintf(stderr, "load_tga(): out of memory while loading '%s'\n", filename);
		fclose(f);
		return NULL;
	}
	internal_format(tga, &header);

	if (header.colormap_type) {
		memset(palette, 0, 256 * 3);

		fread(palette, 1, header.cm_length * header.cm_size / 8, f);

		if (feof(f)) {
			fprintf(stderr, "load_tga(): failed to load '%s'\n", filename);
			fclose(f);
			free(tga);
			return NULL;
		}
	}

/* read image data */
	switch (header.image_type) {
		case 0:				/* no data */
			break;

		case 1:				/* uncompressed 8 bits color index */
			ReadTGA8bits(f, palette, tga);
			break;

		case 2:				/* uncompressed 16-24-32 bits */
			switch(header.pixel_depth) {
				case 16:
					ReadTGA16bits(f, tga);
					break;

				case 24:
					ReadTGA24bits(f, tga);
					break;

				case 32:
					ReadTGA32bits(f, tga);
					break;
			}
			break;

		case 3:				/* uncompressed 8 or 16 bits grayscale */
			if (header.pixel_depth == 8)
				ReadTGAgray8bits(f, tga);
			else 			/* 16 bits gray */
				ReadTGAgray16bits (f, tga);
			break;

		case 9:				/* RLE compressed 8 bits color index */
			ReadTGA8bitsRLE(f, palette, tga);
			break;

		case 10:			/* RLE compressed 16-24-32 bits */
			switch(header.pixel_depth) {
				case 16:
					ReadTGA16bitsRLE(f, tga);
					break;

				case 24:
					ReadTGA24bitsRLE(f, tga);
					break;

				case 32:
					ReadTGA32bitsRLE(f, tga);
					break;
			}
			break;

		case 11:			/* RLE compressed 8 or 16 bits grayscale */
			if (header.pixel_depth == 8)
				ReadTGAgray8bitsRLE(f, tga);
			else			/* 16 */
				ReadTGAgray16bitsRLE(f, tga);
			break;

		default:			/* image type is not correct */
			fprintf(stderr, "error: unknown TGA image type %i in '%s'\n", header.image_type, filename);
			fclose(f);
			free(tga);
			return NULL;
	}
	fclose(f);
	return tga;
}

void free_tga(TGA *tga) {
	if (tga != NULL)
		free(tga);
}

/* EOB */
