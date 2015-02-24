/*
	tga.h	WJ108
*/

#ifndef TGA_H_WJ108
#define TGA_H_WJ108	1

typedef struct {
	int w, h, bytes_per_pixel, format;
	unsigned char pixels[1];
} TGA;


TGA *load_tga(char *);
void free_tga(TGA *);

#endif	/* TGA_H_WJ108 */

/* EOB */
