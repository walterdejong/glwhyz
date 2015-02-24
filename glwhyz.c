/*
	glWHYz! demo	WJ107
*/

#include "tga.h"

#include <SDL.h>
#include <gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <assert.h>

/* enable debug printing */
#define DEBUG	1

#define VERSION				"v2.0"

#define SCREEN_WIDTH		1920
#define SCREEN_HEIGHT		1080
#define SCREEN_BPP			32

#define FPS					30
#define FRAME_DELAY			(1000/FPS)

#define IMG_FG				"images/glwhyz.tga"
#define IMG_BG				"images/smile.tga"
#define IMG_COPYRIGHT		"images/copyright.tga"
#define IMG_PARTICLE		"images/bubble.tga"
#define TEXTURE_FG			0
#define TEXTURE_BG			1
#define TEXTURE_COPYRIGHT	2
#define TEXTURE_PARTICLE	3
#define NUM_TEXTURES		4

/* how dramatic is the wave */
#define WAVE_SCALE			30.0f
/* the wave effect is too fast, WAVE_DELAY is a "frame skip" parameter */
#define WAVE_DELAY			1

/*
	dimensions of the copyright scroller
*/
#define SCROLLER_WIDTH		((GLfloat)SCREEN_WIDTH)
#define SCROLLER_HEIGHT		16.0f
#define SCROLLER_SPEED		2.0f

/* rotating background */
#define ROTATE_SPEED		2.5f

/* defines for the wave form */
#define DIM_X				512
#define DIM_Y				512
#define QUAD_W				16
#define QUAD_H				16
#define DIM_W				(DIM_X / QUAD_W)
#define DIM_H				(DIM_Y / QUAD_H)
#define NUM_VERTEX			((DIM_W+1) * (DIM_H+1))

/* particles */
#define PARTICLE_W			64
#define PARTICLE_H			64
#define NUM_PARTICLES		32
#define PARTICLE_ACCEL		0.1f
#define PARTICLE_DEAD		-1

/* options */
#define OPT_WIREFRAME		1
#define OPT_PAUSED			2
#define OPT_FRAMECOUNTER	4
#define OPT_FULLSCREEN		8


typedef struct {
	GLfloat x, y;
} Vertex;

/* (simple) particles */
typedef struct {
	GLfloat x, y, speed, drift;
	int depth;
} Particle;

SDL_Window *main_window = NULL;
SDL_GLContext glcontext;

const char *title = "glWHYz - WJ107/WJ115";

int options = OPT_FULLSCREEN;

float screen_w = (float)SCREEN_WIDTH;
float screen_h = (float)SCREEN_HEIGHT;
float cam_x = 0.0f, cam_y = 0.0f;

/*
	used for the wave effect:
	* org_vertex holds a large square that has been subdivided into small triangles
	* vertex holds the manipulated triangles, this is what is being drawn
	* texture_vertex holds the texture coordinates
*/
Vertex org_vertex[NUM_VERTEX], texture_vertex[NUM_VERTEX], vertex[NUM_VERTEX];

GLfloat x_offsets[DIM_W+1], y_offsets[DIM_H+1];		/* wave table with offsets */

Uint32 ticks;								/* used for capping the framerate */
int frame_delay = FRAME_DELAY;

GLuint textures[NUM_TEXTURES];				/* GL texture identifiers */

int scroller_depth = 2;						/* the scroller can "move" between layers */

Particle particles[NUM_PARTICLES];			/* particle particles */


void debug(char *fmt, ...) {
#ifdef DEBUG
	va_list ap;
	va_start(ap, fmt);
	printf("TD ");
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
#else
	;
#endif	/* DEBUG */
}

void exit_program(int exit_code) {
	if (main_window != NULL) {
		SDL_GL_DeleteContext(glcontext);
		SDL_DestroyWindow(main_window);
		main_window = NULL;
	}
	SDL_Quit();
	exit(exit_code);
}

/*
	draw the wavy object
*/
void draw_wave(void) {
int i, j, v, n;
GLfloat vertex_arr[(DIM_W+1) * 4], tex_arr[(DIM_W+1) * 4];

	glPushMatrix();

	/* center it */
	glTranslatef(DIM_X - DIM_X * 0.1f, 0.0, 0.0f);
	/* make reasonable size */
	glScalef(2.0f, 2.0f, 1.0f);

	if (options & OPT_WIREFRAME)
		glColor3ub(0xff, 0xff, 0);

	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_FG]);

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	v = 0;
	for(j = 0; j < DIM_H; j++) {
		n = 0;
		v = j * (DIM_W+1);

		tex_arr[n] = texture_vertex[v].x;
		vertex_arr[n++] = vertex[v].x;

		tex_arr[n] = texture_vertex[v].y;
		vertex_arr[n++] = vertex[v].y;

		tex_arr[n] = texture_vertex[v+DIM_W+1].x;
		vertex_arr[n++] = vertex[v+DIM_W+1].x;

		tex_arr[n] = texture_vertex[v+DIM_W+1].y;
		vertex_arr[n++] = vertex[v+DIM_W+1].y;

		for(i = 0; i < DIM_W; i++) {
			tex_arr[n] = texture_vertex[v+1].x;
			vertex_arr[n++] = vertex[v+1].x;

			tex_arr[n] = texture_vertex[v+1].y;
			vertex_arr[n++] = vertex[v+1].y;

			tex_arr[n] = texture_vertex[v+DIM_W+2].x;
			vertex_arr[n++] = vertex[v+DIM_W+2].x;

			tex_arr[n] = texture_vertex[v+DIM_W+2].y;
			vertex_arr[n++] = vertex[v+DIM_W+2].y;

			v++;
		}
		glDrawArrays(GL_TRIANGLE_STRIP, 0, n/2);
	}
	glPopMatrix();
}

/*
	draw a spinning background
*/
void draw_background(void) {
static GLfloat angle = 350.0f;
GLfloat vertex_arr[8] = { 
	-1, 1,
	-1, -1,
	1, 1,
	1, -1
};

GLfloat tex_arr[8] = {
	0, 0,
	0, 1,
	1, 0,
	1, 1
};

	glPushMatrix();
	/* center it */
	glTranslatef(SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, 0.0f);
	/* make reasonable size */
	glScalef(256.0f, 256.0f, 1.0f);

/* spin it around */
	glRotatef(angle, 0.0f, 0.0f, 1.0f);

	if (options & OPT_WIREFRAME)
		glColor3ub(0, 0xa0, 0);
	else
		glColor3f(1.0f, 1.0f, 1.0f);

	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_BG]);

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glPopMatrix();

	if (!(options & OPT_PAUSED)) {
		angle += ROTATE_SPEED;			/* rotate */
		if (angle > 360.0f)
			angle -= 360.0f;
	}
}

void draw_copyright(void) {
static GLfloat x_scroll = SCREEN_WIDTH/2.0f;
static GLfloat direction = -SCROLLER_SPEED;
static GLfloat angle = 0.0f;
int new_depth;

GLfloat vertex_arr[8] = { 
	0, SCROLLER_HEIGHT,
	0, 0,
	SCROLLER_WIDTH, SCROLLER_HEIGHT,
	SCROLLER_WIDTH, 0
};

GLfloat tex_arr[8] = {
	0, 0,
	0, 1,
	1, 0,
	1, 1
};

/* set coordinate system to center of the screen */
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();						/* save old coordinate system */
	glLoadIdentity();
	glOrtho(-SCREEN_WIDTH * 0.5, SCREEN_WIDTH * 0.5, -SCREEN_HEIGHT * 0.5, SCREEN_HEIGHT * 0.5, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

/* the scroller is under an angle */
	if (angle) {
		glRotatef(angle, 0.0f, 0.0f, 1.0f);
		glTranslatef(x_scroll, 0.0f, 0.0f);
	} else {
/* if the angle is 0, put the scroller below in the screen */
		glTranslatef(x_scroll, -(GLfloat)SCREEN_HEIGHT/2 + SCROLLER_HEIGHT, 0.0f);
	}
	glScalef(0.5f, 1.25f, 1.0f);

	if (options & OPT_WIREFRAME)
		glColor3ub(0xff, 0, 0xff);

	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_COPYRIGHT]);

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

/* restore coordinate system */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!(options & OPT_PAUSED)) {
/* when the scroller goes off-screen, restart it under an angle */
		x_scroll += direction;
		if ((x_scroll + SCROLLER_WIDTH + SCROLLER_WIDTH/4) < -SCREEN_WIDTH/2
			|| x_scroll > SCREEN_WIDTH/2 + SCROLLER_WIDTH/4) {
			x_scroll = (GLfloat)SCREEN_WIDTH/2;
			angle = (GLfloat)(rand() % 180 - 90);

			direction = -SCROLLER_SPEED;

			if (rand() & 1) {
				direction = -direction;
				x_scroll = -x_scroll;
				x_scroll -= SCROLLER_WIDTH;
			}
/* set a new scroller depth, do not choose the same depth twice in a row */
			new_depth = rand() % 2;
			if (scroller_depth == new_depth) {
				new_depth++;
				new_depth %= 2;
			}
			scroller_depth = new_depth;
		}
	}
}

void draw_particles(int depth) {
int n;
const GLfloat vertex_arr[8] = {
	0, 0,
	0, PARTICLE_H-1,
	PARTICLE_W-1, 0,
	PARTICLE_W-1, PARTICLE_H-1
};

const GLfloat tex_arr[8] = {
	0, 0,
	0, 1,
	1, 0,
	1, 1
};

GLfloat particle_arr[8];

	glPushMatrix();

	if (options & OPT_WIREFRAME)
		glColor3f(0.0f, 1.0f, 1.0f);
	else {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
	}
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_PARTICLE]);

	memcpy(particle_arr, vertex_arr, sizeof(GLfloat) * 8);
	glVertexPointer(2, GL_FLOAT, 0, particle_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	for(n = 0; n < NUM_PARTICLES; n++) {
		if (particles[n].depth == depth		/* && visible */
			&& particles[n].y > -PARTICLE_H && particles[n].y < SCREEN_HEIGHT) {

			memcpy(particle_arr, vertex_arr, sizeof(GLfloat) * 8);
			particle_arr[0] += particles[n].x;
			particle_arr[1] += particles[n].y;
			particle_arr[2] += particles[n].x;
			particle_arr[3] += particles[n].y;
			particle_arr[4] += particles[n].x;
			particle_arr[5] += particles[n].y;
			particle_arr[6] += particles[n].x;
			particle_arr[7] += particles[n].y;

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}
	glDisable(GL_BLEND);

	glPopMatrix();
}

void draw_scene(void) {
	glClear(GL_COLOR_BUFFER_BIT);

	glLoadIdentity();
	/* reverse camera */
	glTranslatef(-cam_x, -cam_y, 0.0f);

	draw_particles(0);

	if (!scroller_depth)
		draw_copyright();

	draw_particles(1);

	draw_background();

	draw_particles(2);

	if (scroller_depth == 1)
		draw_copyright();

	draw_particles(3);

	draw_wave();

	draw_particles(4);

	if (scroller_depth == 2)
		draw_copyright();

	draw_particles(5);

	glFlush();
	GLenum err = glGetError();
	if (err != 0) {
		fprintf(stderr, "glGetError(): %d\n", (int)err);
		exit_program(-1);
	}
}

void init_gl(void) {
	debug("init_gl()");

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_TEXTURE_2D);
	glColor3f(1.0f, 1.0f, 1.0f);
	glPolygonMode(GL_FRONT, GL_FILL);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void create_window(int w, int h) {
	if (w <= 0)
		w = 1;

	if (h <= 0)
		h = 1;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	Uint32 modeflags = SDL_WINDOW_OPENGL;
	if (options & OPT_FULLSCREEN) {
		modeflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		w = h = 0;
	} else {
		modeflags |= SDL_WINDOW_RESIZABLE;

	}
	main_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, 0, w, h, modeflags);
	if (main_window == NULL) {
		fprintf(stderr, "failed to create window: %s\n", SDL_GetError());
		exit(-1);
	}
	glcontext = SDL_GL_CreateContext(main_window);

	SDL_GL_SetSwapInterval(1);

	/* get screen dimensions */
	if (modeflags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		SDL_DisplayMode mode;

		SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		/* get display dimensions */
		SDL_GetDisplayMode(0, 0, &mode);
		w = mode.w;
		h = mode.h;
	} else {
		SDL_GetWindowSize(main_window, &w, &h);
	} 
	debug("screen dimensions: %dx%d", w, h);
	screen_w = (float)w;
	screen_h = (float)h;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

/* use "screen" coordinate system */
	glOrtho(0.0, (GLdouble)SCREEN_WIDTH, (GLdouble)SCREEN_HEIGHT, 0.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glViewport(0, 0, w, h);
}

/*
	delay a while
	It's not exact, and not trying to be exact
*/
void cap_framerate(void) {
	SDL_Delay(FRAME_DELAY);
}

void count_framerate(void) {
static int fps = 0;
Uint32 new_ticks;

	fps++;

	new_ticks = SDL_GetTicks();

	if (new_ticks - ticks >= 1000) {
		printf("%d FPS\n", fps);
		fps = 0;
		ticks = new_ticks;
	}
}

/*
	define vertices and set wave table values
*/
void init_wave(void) {
int i, j, n;
double val, d;

/* set coordinates for polygons and for texturing */
	n = 0;
	for(j = 0; j < DIM_H+1; j++) {
		for(i = 0; i < DIM_W+1; i++) {
			org_vertex[n].x = i * QUAD_W;
			org_vertex[n].y = j * QUAD_H;
			texture_vertex[n].x = (GLfloat)i / (GLfloat)DIM_W;
			texture_vertex[n].y = (GLfloat)j / (GLfloat)DIM_H;
			n++;
		}
	}
/* initialize wave tables */
	for(n = 0; n < DIM_W+1; n++) {
		val = (double)(n * 2 * M_PI) / (double)(DIM_W+1);
		d = sin(val*2) + cos(val)/2.0;
		x_offsets[n] = WAVE_SCALE * d;
	}
	for(n = 0; n < DIM_H+1; n++) {
		val = (double)(n * 2 * M_PI) / (double)(DIM_H+1);
		d = sin(val) + cos(3*val)/3.0 + sin(2*val)/2.0;
		y_offsets[n] = WAVE_SCALE * d;
	}
}

/*
	on every frame, add wave table values to the coordinates of the triangles
*/
void animate(void) {
static int xt = 0, yt = 0, delay = 0;
int i, j, n;

#if WAVE_DELAY
	if (delay) {				/* slow down! the wave is too fast */
		delay++;
		if (delay >= WAVE_DELAY)
			delay = 0;
		return;
	} else
		delay++;
#endif

	memcpy(vertex, org_vertex, sizeof(Vertex) * NUM_VERTEX);

	n = 0;
	for(j = 0; j < DIM_H+1; j++) {
		for(i = 0; i < DIM_W+1; i++) {
			vertex[n].x += x_offsets[xt];
			vertex[n].y += y_offsets[yt];
			yt++;
			if (yt >= DIM_H+1)
				yt = 0;

			n++;
		}
		xt++;
		if (xt >= DIM_W+1)
			xt = 0;
	}
	xt++;
	if (xt >= DIM_W+1)
		xt = 0;

	yt++;
	if (yt >= DIM_H+1)
		yt = 0;
}

void init_particle(int n) {
/*
	these particles move in the Y direction and hardly in the X direction
	allowing them to start far off the screen is my way of spreading them out better
	it's not perfect, but it works somewhat
*/
	particles[n].x = rand() % (SCREEN_WIDTH - PARTICLE_W);
	particles[n].y = SCREEN_HEIGHT + PARTICLE_H + (rand() % SCREEN_HEIGHT);
	particles[n].speed = (rand() % 5) + 5.0f;
	particles[n].drift = (rand() % 5) - 2.0f / 2.0f;
	particles[n].depth = rand() % 6;

	particles[n].speed = particles[n].depth * 0.5f + 1.0f;	/* speed based on depth */
}

void init_particles(void) {
int n;

	for(n = 0; n < NUM_PARTICLES; n++)
		particles[n].depth = PARTICLE_DEAD;
}

void move_particles(void) {
int n;

	for(n = 0; n < NUM_PARTICLES; n++) {
		if (particles[n].depth != PARTICLE_DEAD) {
			particles[n].y -= particles[n].speed;
			particles[n].speed += PARTICLE_ACCEL;		/* acceleration */

			particles[n].x += particles[n].drift;

			if (particles[n].y <= -PARTICLE_H)
				particles[n].depth = PARTICLE_DEAD;
			else {
				if (particles[n].x <= -PARTICLE_W)
					particles[n].depth = PARTICLE_DEAD;
				else
					if (particles[n].x >= SCREEN_WIDTH)
						particles[n].depth = PARTICLE_DEAD;
			}
		} else
			init_particle(n);
	}
}

void draw_screen(void) {
	draw_scene();

	SDL_GL_SwapWindow(main_window);

	if (options & OPT_FRAMECOUNTER)
		count_framerate();
	else
		cap_framerate();
}

void handle_keypress(int key) {
int flags;

	switch(key) {
		case SDLK_ESCAPE:
			exit_program(0);

		case SDLK_w:
			options ^= OPT_WIREFRAME;

/* show wireframe polygons */
			if (options & OPT_WIREFRAME) {
				glDisable(GL_TEXTURE_2D);
				glColor3f(1.0f, 0.0f, 0.0f);
				glPolygonMode(GL_FRONT, GL_LINE);
			} else {
				glEnable(GL_TEXTURE_2D);
				glColor3f(1.0f, 1.0f, 1.0f);
				glPolygonMode(GL_FRONT, GL_FILL);
			}
			break;

		case SDLK_PAUSE:
		case SDLK_p:
			options ^= OPT_PAUSED;
			break;

		case SDLK_KP_MINUS:
		case SDLK_MINUS:
			frame_delay++;
			break;

		case SDLK_KP_PLUS:
		case SDLK_PLUS:
			frame_delay--;
			if (frame_delay < 1)
				frame_delay = 1;
			break;

		case SDLK_f:
			options ^= OPT_FRAMECOUNTER;
			break;

		case SDLK_RETURN:
			flags = SDL_GetWindowFlags(main_window);
			if (flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP)) {
				SDL_SetWindowFullscreen(main_window, 0);
				screen_w = (float)SCREEN_WIDTH;
				screen_h = (float)SCREEN_HEIGHT;
				glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
			} else {
				SDL_DisplayMode mode;

				SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
				/* get display dimensions */
				SDL_GetDisplayMode(0, 0, &mode);
				debug("fullscreen dimensions: %dx%d", mode.w, mode.h);
				screen_w = (float)mode.w;
				screen_h = (float)mode.h;
				glViewport(0, 0, mode.w, mode.h);
			}
			break;

		default:
			;
	}
}

void handle_keyrelease(int key) {
	;
}

void handle_events(void) {
SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_WINDOWEVENT_CLOSE:
				debug("CLOSE");
				exit_program(0);

			case SDL_WINDOWEVENT_SHOWN:
				debug("SHOWN");
				draw_screen();
				break;

			case SDL_WINDOWEVENT_EXPOSED:
				debug("EXPOSED");
				draw_screen();
				break;

			case SDL_WINDOWEVENT_RESIZED:
				debug("RESIZED");
				break;

			case SDL_WINDOWEVENT_FOCUS_LOST:
				debug("FOCUS_LOST");
				SDL_WaitEvent(NULL);		/* freeze it */
				break;

			case SDL_KEYDOWN:
				handle_keypress(event.key.keysym.sym);
				break;

			case SDL_KEYUP:
				handle_keyrelease(event.key.keysym.sym);
				break;

			default:
				;
		}
	}
}

int load_texture(char *filename, GLuint ident) {
TGA *tga;
int format;

	debug("load_texture(%s, %u)", filename, ident);

	if ((tga = load_tga(filename)) == NULL) {
		fprintf(stderr, "failed to load texture file '%s'\n", filename);
		return -1;
	}
	switch(tga->bytes_per_pixel) {
/*
	Not supported in OpenGL ES

		case 1:
			format = GL_COLOR_INDEX;
			break;

		case 3:
			format = GL_BGR;
			break;
*/
		case 4:
			format = GL_RGBA;
			break;

		default:
			format = -1;		/* invalid */
	}
	glBindTexture(GL_TEXTURE_2D, ident);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tga->w, tga->h, 0, format, GL_UNSIGNED_BYTE, tga->pixels);

	free_tga(tga);
	return 0;
}

int main(int argc, char *argv[]) {
	printf("glWHYz! demo " VERSION " - Copyright (C) 2007 2015 by Walter de Jong <walter@heiho.net>\n");

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init() failed\n");
		return -1;
	}
	create_window(SCREEN_WIDTH, SCREEN_HEIGHT);

	init_gl();
	glGenTextures(NUM_TEXTURES, textures);
	if (load_texture(IMG_BG, textures[TEXTURE_BG]))
		exit_program(-1);

	if (load_texture(IMG_FG, textures[TEXTURE_FG]))
		exit_program(-1);

	if (load_texture(IMG_COPYRIGHT, textures[TEXTURE_COPYRIGHT]))
		exit_program(-1);

	if (load_texture(IMG_PARTICLE, textures[TEXTURE_PARTICLE]))
		exit_program(-1);

	init_wave();
	init_particles();

	srand(time(NULL));

	ticks = SDL_GetTicks();
	draw_screen();

	for(;;) {
		handle_events();

		if (!(options & OPT_PAUSED)) {
			animate();
			move_particles();
		}
		draw_screen();
	}
	SDL_Quit();
	return 0;
}

/* EOB */
