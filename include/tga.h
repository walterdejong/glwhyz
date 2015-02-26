/*
	tga.h	WJ108
*/

#ifndef TGA_H_WJ108
#define TGA_H_WJ108	1

typedef struct {
	int w, h, bytes_per_pixel, format;
	unsigned char pixels[1];
} TGA;


#ifdef __cplusplus
extern "C" {
#endif

TGA *load_tga(const char *);
void free_tga(TGA *);

#ifdef __cplusplus
};
#endif

#endif	/* TGA_H_WJ108 */

/* EOB */
