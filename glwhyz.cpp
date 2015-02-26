//
//	glWHYz! demo	WJ107
//

#include "tga.h"

#include <SDL.h>
#include <gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>

// enable debug printing
//#define DEBUG	1

#define VERSION				"v2.0"

#define SCREEN_WIDTH		1920
#define SCREEN_HEIGHT		1080
#define SCREEN_BPP			32

#define IMG_FG				"images/glwhyz.tga"
#define IMG_BG				"images/smile.tga"
#define IMG_COPYRIGHT		"images/copyright.tga"
#define IMG_PARTICLE		"images/bubble.tga"
#define TEXTURE_FG			0
#define TEXTURE_BG			1
#define TEXTURE_COPYRIGHT	2
#define TEXTURE_PARTICLE	3
#define NUM_TEXTURES		4

// how dramatic is the wave
#define WAVE_SCALE			30.0f
// the wave effect is too fast, WAVE_DELAY is a "frame skip" parameter
#define WAVE_DELAY			1

//	dimensions of the copyright scroller
#define SCROLLER_WIDTH		((GLfloat)SCREEN_WIDTH)
#define SCROLLER_HEIGHT		16.0f
#define SCROLLER_SPEED		10.0f
#define SCROLLER_SCALE_X	0.5f
#define SCROLLER_SCALE_Y	1.25f

// rotating background
#define ROTATE_SPEED		120.0f

// defines for the wave form
#define DIM_X				512
#define DIM_Y				512
#define QUAD_W				16
#define QUAD_H				16
#define DIM_W				(DIM_X / QUAD_W)
#define DIM_H				(DIM_Y / QUAD_H)
#define NUM_VERTEX			((DIM_W+1) * (DIM_H+1))
#define WAVE_SPEED			16.0f

// particles
#define PARTICLE_W			64
#define PARTICLE_H			64
#define NUM_PARTICLES		32
#define PARTICLE_ACCEL		0.1f
#define PARTICLE_DEAD		-1
#define PARTICLE_SPEED		30.0f

// options
#define OPT_WIREFRAME		1
#define OPT_PAUSED			2
#define OPT_FRAMECOUNTER	4
#define OPT_FULLSCREEN		8

struct Vertex {
	GLfloat x, y;
};

class Wave {
	// TODO add x, y
	int xt, yt;
	float xtime, ytime;
/*
	used for the wave effect:
	* org_vertex holds a large square that has been subdivided into small triangles
	* vertex holds the manipulated triangles, this is what is being drawn
	* texture_vertex holds the texture coordinates
*/
	Vertex org_vertex[NUM_VERTEX], vertex[NUM_VERTEX];
	Vertex texture_vertex[NUM_VERTEX];

	GLfloat x_offsets[DIM_W+1], y_offsets[DIM_H+1];		// table with offsets

public:
	void init(void);
	void animate(float);
	void draw(void);
};

class Spinner {
	// TODO add x, y
	// TODO add per-object angular direction, speed?
	float angle;

public:
	Spinner() : angle(0.0f) { }

	void spin(float);
	void draw(void);
};

// copyright scroller
class Scroller {
	GLfloat x;
	GLfloat direction;
	GLfloat angle;

public:
	int depth;		// the scroller can "move" between layers

	void init(void);
	void move(float);
	void draw(void);
};

// (very simple) particles
struct Particle {
	GLfloat x, y, speed, drift;
	int depth;

	void init(void);
	void move(float);
	void draw(void);
};

class ParticleSystem {
	Particle parts[NUM_PARTICLES];

public:
	void init(void);
	void move(float);
	void draw(int);
};

SDL_Window *main_window = NULL;
SDL_GLContext glcontext;

const char *title = "glWHYz - WJ107/WJ115";

int options = 0;	// OPT_FULLSCREEN;

float screen_w = (float)SCREEN_WIDTH;
float screen_h = (float)SCREEN_HEIGHT;
// camera does nothing, really
float cam_x = 0.0f, cam_y = 0.0f;

GLuint textures[NUM_TEXTURES];				// GL texture identifiers

Wave wave;
Spinner spinner;
Scroller scroller;
ParticleSystem particles;

float perf_freq;
int framecount = 0;
Uint64 framecount_timer;
Uint64 last_time;


void debug(const char *fmt, ...) {
#ifdef DEBUG
	va_list ap;

	va_start(ap, fmt);
	printf("TD ");
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
#else
	;
#endif	// DEBUG
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

//	define vertices and set wave table values
void Wave::init(void) {
	// set coordinates for polygons and for texturing
	for(int j = 0, n = 0; j < DIM_H+1; j++) {
		for(int i = 0; i < DIM_W+1; i++) {
			org_vertex[n].x = i * QUAD_W;
			org_vertex[n].y = j * QUAD_H;
			texture_vertex[n].x = (GLfloat)i / (GLfloat)DIM_W;
			texture_vertex[n].y = (GLfloat)j / (GLfloat)DIM_H;
			n++;
		}
	}
	// initialize wave tables
	for(int n = 0; n < DIM_W+1; n++) {
		double val = (double)(n * 2 * M_PI) / (double)(DIM_W+1);
		double d = sin(val*2) + cos(val)/2.0;
		x_offsets[n] = WAVE_SCALE * d;
	}
	for(int n = 0; n < DIM_H+1; n++) {
		double val = (double)(n * 2 * M_PI) / (double)(DIM_H+1);
		double d = sin(val) + cos(3*val)/3.0 + sin(2*val)/2.0;
		y_offsets[n] = WAVE_SCALE * d;
	}
	xt = yt = 0;
}

//	on every frame, add wave table values to the coordinates of the triangles
void Wave::animate(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	// update wave vertices
	memcpy(vertex, org_vertex, sizeof(Vertex) * NUM_VERTEX);

	for(int j = 0, n = 0; j < DIM_H+1; j++) {
		for(int i = 0; i < DIM_W+1; i++) {
			vertex[n].x += x_offsets[wave.xt];
			vertex[n].y += y_offsets[wave.yt];
			yt++;
			if (yt >= DIM_H+1) {
				yt = 0;
			}
			n++;
		}
		xt++;
		if (xt >= DIM_W+1) {
			xt = 0;
		}
	}

	/* this steers the wavy animation */
	xtime += timestep * WAVE_SPEED;
	if (xtime >= 1.0f) {
		xtime -= 1.0f;
		xt++;
		if (xt >= DIM_W+1) {
			xt = 0;
		}
	}
	ytime += timestep * WAVE_SPEED;
	if (ytime >= 1.0f) {
		ytime -= 1.0f;
		yt++;
		if (yt >= DIM_H+1) {
			yt = 0;
		}
	}
}

// draw the wavy object
void Wave::draw(void) {
	glPushMatrix();

	// FIXME translate(x, y, 0)
	// center it
	glTranslatef(DIM_X - DIM_X * 0.1f, 0.0, 0.0f);
	// make reasonable size
	glScalef(2.0f, 2.0f, 1.0f);		// FIXME scaling parameter

	if (options & OPT_WIREFRAME) {
		glColor3ub(0xff, 0xff, 0);
	}
	// FIXME texture_idx member
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_FG]);

	GLfloat vertex_arr[(DIM_W+1) * 4];
	GLfloat tex_arr[(DIM_W+1) * 4];

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	for(int j = 0, n = 0, v = 0; j < DIM_H; j++) {
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

		for(int i = 0; i < DIM_W; i++) {
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

void Spinner::spin(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	if (!(options & OPT_PAUSED)) {
		angle += ROTATE_SPEED * timestep;	// rotate
		if (angle >= 360.0f)
			angle -= 360.0f;
	}
}

// draw a spinner
void Spinner::draw(void) {
	glPushMatrix();
	// center it
	glTranslatef(SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, 0.0f);
	// make reasonable size
	glScalef(256.0f, 256.0f, 1.0f);

	// spin it around
	glRotatef(angle, 0.0f, 0.0f, 1.0f);

	if (options & OPT_WIREFRAME) {
		glColor3ub(0, 0xa0, 0);
	} else {
		glColor3f(1.0f, 1.0f, 1.0f);
	}
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_BG]);

	const GLfloat vertex_arr[8] = { 
		-1, 1,
		-1, -1,
		1, 1,
		1, -1
	};

	const GLfloat tex_arr[8] = {
		0, 0,
		0, 1,
		1, 0,
		1, 1
	};

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glPopMatrix();
}

void Scroller::init(void) {
	x = SCREEN_WIDTH * 0.5f,
	direction = -SCROLLER_SPEED,
	angle = 0.0f,
	depth = 2;		// the scroller can "move" between layers
}

void Scroller::move(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	if (options & OPT_PAUSED) {
		return;
	}
	x += direction * timestep * SCROLLER_SPEED;

	// when the scroller goes off-screen, restart it under an angle
	if ((x + SCROLLER_WIDTH + SCROLLER_WIDTH * 0.25f) < -SCREEN_WIDTH * 0.5f
		|| x > SCREEN_WIDTH * 0.5f + SCROLLER_WIDTH * 0.25f) {
		x = (GLfloat)SCREEN_WIDTH * 0.5f;
		angle = (GLfloat)(random() % 180 - 90);
		direction = -SCROLLER_SPEED;

		if (random() & 1) {
			direction = -direction;
			x = -x;
			x -= SCROLLER_WIDTH;
		}
		// set a new scroller depth, do not choose the same depth twice in a row
		int new_depth = random() % 2;
		if (depth == new_depth) {
			new_depth++;
			new_depth %= 2;
		}
		depth = new_depth;
	}
}

void Scroller::draw(void) {
	// set coordinate system to center of the screen
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();						// save old coordinate system
	glLoadIdentity();
	glOrtho(-SCREEN_WIDTH * 0.5, SCREEN_WIDTH * 0.5, -SCREEN_HEIGHT * 0.5, SCREEN_HEIGHT * 0.5, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// the scroller is under an angle
	if (angle > 0.01f) {
		glRotatef(angle, 0.0f, 0.0f, 1.0f);
		glTranslatef(x, 0.0f, 0.0f);
	} else {
		// if the angle is 0, put the scroller below in the screen
		glTranslatef(x, -(GLfloat)SCREEN_HEIGHT * 0.5f + SCROLLER_HEIGHT, 0.0f);
	}
	glScalef(SCROLLER_SCALE_X, SCROLLER_SCALE_Y, 1.0f);

	if (options & OPT_WIREFRAME) {
		glColor3ub(0xff, 0, 0xff);
	}
	// scroller text is an image texture
	// could have been TTF font rendering ...
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_COPYRIGHT]);

	const GLfloat vertex_arr[8] = { 
		0, SCROLLER_HEIGHT,
		0, 0,
		SCROLLER_WIDTH, SCROLLER_HEIGHT,
		SCROLLER_WIDTH, 0
	};

	const GLfloat tex_arr[8] = {
		0, 0,
		0, 1,
		1, 0,
		1, 1
	};

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// restore coordinate system
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void Particle::init(void) {
	/*
		these particles move in the Y direction and hardly in the X direction
		allowing them to start far off the screen is my way of spreading them out better
		it's not perfect, but it works somewhat
	*/
	x = random() % (SCREEN_WIDTH - PARTICLE_W);
	y = SCREEN_HEIGHT + PARTICLE_H + (random() % SCREEN_HEIGHT);
	speed = (random() % 5) + 5.0f;
	drift = (random() % 5) * 0.5f - 1.0f;
	depth = random() % 6;
	speed = depth * 0.5f + 1.0f;	// speed based on depth
}

void Particle::move(float timestep) {
	if (depth != PARTICLE_DEAD) {
		y -= speed * timestep * PARTICLE_SPEED;
		speed += PARTICLE_ACCEL;	// acceleration

		x += drift * timestep * PARTICLE_SPEED;

		if (y <= -PARTICLE_H) {
			depth = PARTICLE_DEAD;
		} else {
			if (x <= -PARTICLE_W) {
				depth = PARTICLE_DEAD;
			} else {
				if (x >= SCREEN_WIDTH) {
					depth = PARTICLE_DEAD;
				}
			}
		}
	} else {
		init();
	}
}

void Particle::draw(void) {
	// Note: texture must already have been bound
	// Note: texture coord pointer must already have been set

	GLfloat vertex_arr[8];

	vertex_arr[0] = x;
	vertex_arr[1] = y;
	vertex_arr[2] = x;
	vertex_arr[3] = y + PARTICLE_H;
	vertex_arr[4] = x + PARTICLE_W;
	vertex_arr[5] = y;
	vertex_arr[6] = x + PARTICLE_W;
	vertex_arr[7] = y + PARTICLE_H;

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ParticleSystem::init(void) {
	for(int n = 0; n < NUM_PARTICLES; n++) {
		parts[n].depth = PARTICLE_DEAD;
	}
}

void ParticleSystem::move(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	for(int n = 0; n < NUM_PARTICLES; n++) {
		parts[n].move(timestep);
	}
}

void ParticleSystem::draw(int depth) {
	glPushMatrix();

	if (options & OPT_WIREFRAME) {
		glColor3f(0.0f, 1.0f, 1.0f);
	} else {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
	}
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_PARTICLE]);

	const GLfloat tex_arr[8] = {
		0, 0,
		0, 1,
		1, 0,
		1, 1
	};

	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	for(int n = 0; n < NUM_PARTICLES; n++) {
		if (parts[n].depth == depth		/* && visible */
			&& parts[n].y > -PARTICLE_H && parts[n].y < SCREEN_HEIGHT) {
			parts[n].draw();
		}
	}
	glDisable(GL_BLEND);

	glPopMatrix();
}

void draw_scene(void) {
	glClear(GL_COLOR_BUFFER_BIT);

	glLoadIdentity();
	// reverse camera
	glTranslatef(-cam_x, -cam_y, 0.0f);

	particles.draw(0);

	if (!scroller.depth) {
		scroller.draw();
	}
	particles.draw(1);

	spinner.draw();

	particles.draw(2);

	if (scroller.depth == 1) {
		scroller.draw();
	}
	particles.draw(3);

	wave.draw();

	particles.draw(4);

	if (scroller.depth == 2) {
		scroller.draw();
	}
	particles.draw(5);
}

void count_framerate(void) {
	framecount++;
	if (framecount > 100) {
		Uint64 now = SDL_GetPerformanceCounter();
		printf("FPS %.1f\n", (float)framecount / ((now - framecount_timer) / perf_freq));
		framecount_timer = now;
		framecount = 0;
	}
}

void draw_screen(void) {
	draw_scene();

	glFlush();
	GLenum err = glGetError();
	if (err != 0) {
		fprintf(stderr, "glGetError(): %d\n", (int)err);
		exit_program(-1);
	}
	SDL_GL_SwapWindow(main_window);

	if (options & OPT_FRAMECOUNTER) {
		count_framerate();
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
	if (w <= 0) {
		w = 1;
	}
	if (h <= 0) {
		h = 1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	Uint32 modeflags = SDL_WINDOW_OPENGL;
	if (options & OPT_FULLSCREEN) {
		/*
		 * note: SDL_WINDOW_FULLSCREEN may produce weird results
		 * like having overscan bands
		 */
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

	// get screen dimensions
	if (modeflags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		SDL_DisplayMode mode;

		SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		// get display dimensions
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

	// use "screen" coordinate system
	glOrtho(0.0, (GLdouble)SCREEN_WIDTH, (GLdouble)SCREEN_HEIGHT, 0.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glViewport(0, 0, w, h);
}

void window_event(int event) {
	switch(event) {
		case SDL_WINDOWEVENT_CLOSE:
			debug("CLOSE");
			exit_program(0);

		case SDL_WINDOWEVENT_SHOWN:
			debug("SHOWN");
			options &= ~OPT_PAUSED;
			draw_screen();
			break;

		case SDL_WINDOWEVENT_HIDDEN:
			debug("HIDDEN");
			options |= OPT_PAUSED;
			break;

		case SDL_WINDOWEVENT_EXPOSED:
			debug("EXPOSED");
			draw_screen();
			break;

		case SDL_WINDOWEVENT_RESIZED:
			debug("RESIZED");
			int w, h;
			SDL_GetWindowSize(main_window, &w, &h);
			debug("window size: %dx%d", w, h);
			screen_w = (float)w;
			screen_h = (float)h;
			glViewport(0, 0, screen_w, screen_h);
			break;

		case SDL_WINDOWEVENT_FOCUS_LOST:
			debug("FOCUS_LOST");
			options |= OPT_PAUSED;
			break;

		case SDL_WINDOWEVENT_FOCUS_GAINED:
			debug("FOCUS_GAINED");
			options &= ~OPT_PAUSED;
			break;

		default:
			;
	}
}

void handle_keypress(int key) {
	int flags = 0;

	switch(key) {
		case SDLK_ESCAPE:
			exit_program(0);

		case SDLK_w:
			options ^= OPT_WIREFRAME;

			// show wireframe polygons
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

		case SDLK_f:
			options ^= OPT_FRAMECOUNTER;
			break;

		case SDLK_RETURN:
			if (options & OPT_FULLSCREEN) {
				// doesn't work; can't set window size; SDL bug?
				break;
			}
			flags = SDL_GetWindowFlags(main_window);
			if (flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP)) {
				SDL_SetWindowFullscreen(main_window, 0);
				screen_w = (float)SCREEN_WIDTH;
				screen_h = (float)SCREEN_HEIGHT;
				glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
			} else {
				SDL_DisplayMode mode;

				SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
				// get display dimensions
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

	if (options & OPT_PAUSED) {
		SDL_WaitEvent(NULL);

		// reset time
		last_time = SDL_GetPerformanceCounter();
	}

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_WINDOWEVENT:
				window_event(event.window.event);
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

int load_texture(const char *filename, GLuint ident) {
	debug("load_texture(%s, %u)", filename, ident);

	TGA *tga;

	if ((tga = load_tga(filename)) == NULL) {
		fprintf(stderr, "failed to load texture file '%s'\n", filename);
		return -1;
	}

	int format;

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
			format = -1;	// invalid
			fprintf(stderr, "error: %s: invalid bytes per pixel: %d\n", filename, tga->bytes_per_pixel);
			return -1;
	}
	glBindTexture(GL_TEXTURE_2D, ident);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tga->w, tga->h, 0, format, GL_UNSIGNED_BYTE, tga->pixels);

	free_tga(tga);
	return 0;
}

int main(int argc, const char *argv[]) {
	printf("glWHYz! demo " VERSION " - Copyright (C) 2007 2015 by Walter de Jong <walter@heiho.net>\n");

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init() failed\n");
		return -1;
	}
	create_window(SCREEN_WIDTH, SCREEN_HEIGHT);

	init_gl();
	glGenTextures(NUM_TEXTURES, textures);
	if (load_texture(IMG_BG, textures[TEXTURE_BG]) ||
		load_texture(IMG_FG, textures[TEXTURE_FG]) ||
		load_texture(IMG_COPYRIGHT, textures[TEXTURE_COPYRIGHT]) ||
		load_texture(IMG_PARTICLE, textures[TEXTURE_PARTICLE])) {
		exit_program(-1);
	}
	wave.init();
	scroller.init();
	particles.init();

	srandom(time(NULL));

	perf_freq = SDL_GetPerformanceFrequency();
	debug("perf_freq: %f", perf_freq);
	last_time = framecount_timer = SDL_GetPerformanceCounter();
	draw_screen();
	for(;;) {
		handle_events();

		Uint64 now = SDL_GetPerformanceCounter();
		float timestep = (now - last_time) / perf_freq;
//		debug("timestep %f", timestep);

		if (!(options & OPT_PAUSED)) {
			wave.animate(timestep);
			spinner.spin(timestep);
			particles.move(timestep);
			scroller.move(timestep);
		}
		draw_screen();

		last_time = now;
	}
	SDL_Quit();
	return 0;
}

// EOB