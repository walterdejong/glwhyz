//
//	glWHYz! demo	WJ107
//

#include "tga.h"

#include <SDL.h>
#include <gl.h>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cmath>
#include <cassert>

using namespace std;

// enable debug printing
//#define DEBUG	1

#define VERSION				"v2.0"

#define SCREEN_WIDTH		1920
#define SCREEN_HEIGHT		1080
#define SCREEN_BPP			32

// defines for the wave form
#define QUAD_W				16
#define QUAD_H				16
#define DIM_W				32
#define DIM_H				32
#define NUM_VERTEX			((DIM_W+1) * (DIM_H+1))
#define WAVE_SPEED			16.0f
// how dramatic is the wave
#define WAVE_SCALE			30.0f

// options
#define OPT_WIREFRAME		1
#define OPT_PAUSED			2
#define OPT_FRAMECOUNTER	4
#define OPT_FULLSCREEN		8

class TextureMgr {
public:
	static const int TEX_WAVE = 0;
	static const int TEX_SMILEY = 1;
	static const int TEX_SCROLLER = 2;
	static const int TEX_BUBBLE = 3;

	TextureMgr() : num_textures(0), textures(nullptr) { }
	~TextureMgr() {
		if (textures != nullptr) {
			delete [] textures;
			textures = nullptr;
		}
	}

	bool load(int, const char **);
	void glbind(int);

private:
	GLsizei num_textures;
	GLuint *textures;	// GL texture identifiers

	bool load_texture(const char *, int);
};

struct Vec2 {
	GLfloat x, y;
};

class Wave {
	float x, y;
	int xt, yt;
	float xtime, ytime;
	float scale;
	int texture;
/*
	used for the wave effect:
	* org_vertex holds a large square that has been subdivided into small triangles
	* vertex holds the manipulated triangles, this is what is being drawn
	* texture_vertex holds the texture coordinates
*/
	Vec2 org_vertex[NUM_VERTEX], vertex[NUM_VERTEX];
	Vec2 texture_vertex[NUM_VERTEX];

	GLfloat x_offsets[DIM_W+1], y_offsets[DIM_H+1];		// table with offsets

public:
	Wave() : x((SCREEN_WIDTH - (DIM_W + WAVE_SCALE * DIM_W)) * 0.5f),
		y((SCREEN_HEIGHT - (DIM_H + WAVE_SCALE * DIM_H)) * 0.5f),
		xt(0), yt(0), xtime(0.0f), ytime(0.0), scale(2.0f), texture(TextureMgr::TEX_WAVE) { }

	void init(void);
	void animate(float);
	void draw(void);
};

class Spinner {
public:
	float x, y, scale, rotation, angle;

	Spinner() : x(SCREEN_WIDTH * 0.5f), y(SCREEN_HEIGHT * 0.5f),
		scale(256.0f), rotation(120.0f), angle(0.0f) { }

	void spin(float);
	void draw(void);
};

// copyright scroller
class Scroller {
	static constexpr float WIDTH = SCREEN_WIDTH;
	static constexpr float HEIGHT = 16.0f;
	static constexpr float SPEED = 10.0f;
	static constexpr float SCALE_X = 0.5f;
	static constexpr float SCALE_Y = 1.25f;

	GLfloat x;
	GLfloat direction;
	GLfloat angle;
	int texture;

public:
	int depth;		// the scroller can "move" between layers

	Scroller() : x(SCREEN_WIDTH * 0.5f), direction(-Scroller::SPEED), angle(0.0f),
		texture(TextureMgr::TEX_SCROLLER), depth(2) { }

	void move(float);
	void draw(void);
};

// (very simple) particles
struct Particle {
// particles
	static const int WIDTH = 64;
	static const int HEIGHT = 64;
	static constexpr float ACCEL = 0.1f;
	static const int DEAD = -1;
	static constexpr float SPEED = 30.0f;

	float x, y, speed, drift;
	int depth;

	void init(void);
	void move(float);
	void draw(void);
};

class ParticleSystem {
	static const int NUM_PARTICLES = 32;
	Particle parts[ParticleSystem::NUM_PARTICLES];

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

TextureMgr texmgr;
const char *texture_filenames[] = {
	"images/glwhyz.tga",
	"images/smile.tga",
	"images/copyright.tga",
	"images/bubble.tga",
};

Wave wave;
Spinner spinner, small_spinner;
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

bool TextureMgr::load(int num, const char **filenames) {
	assert(num > 0);
	assert(filenames != nullptr);

	textures = new GLuint[num];

	glGenTextures(num, textures);
	if (glGetError() != 0) {
		delete [] textures;
		textures = nullptr;
		return false;
	}
	num_textures = num;

	for(int i = 0; i < num_textures; i++) {
		if (!load_texture(filenames[i], i)) {
			// all or nothing
			// maybe we can do better?
			delete [] textures;
			textures = nullptr;
			num_textures = 0;
			return false;
		}
	}
	return true;
}

void TextureMgr::glbind(int idx) {
	assert(idx >= 0 && idx < num_textures);
	glBindTexture(GL_TEXTURE_2D, textures[idx]);
}

bool TextureMgr::load_texture(const char *filename, int idx) {
	assert(filename != nullptr);
	assert(idx >= 0 && idx < num_textures);

	debug("TextureMgr::load_texture(%s, %d)", filename, idx);

	TGA *tga;
	if ((tga = load_tga(filename)) == nullptr) {
		fprintf(stderr, "failed to load texture file '%s'\n", filename);
		return false;
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
			return false;
	}
	glbind(idx);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tga->w, tga->h, 0, format, GL_UNSIGNED_BYTE, tga->pixels);

	free_tga(tga);
	return true;
}

//	define vertices and set wave table values
void Wave::init(void) {
	float center_x = DIM_W * 0.5f;
	float center_y = DIM_H * 0.5f;

	// set coordinates for polygons and for texturing
	for(int j = 0, n = 0; j < DIM_H+1; j++) {
		for(int i = 0; i < DIM_W+1; i++) {
			org_vertex[n].x = i * QUAD_W - center_x;
			org_vertex[n].y = j * QUAD_H - center_y;
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
}

//	on every frame, add wave table values to the coordinates of the triangles
void Wave::animate(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	// update wave vertices
	memcpy(vertex, org_vertex, sizeof(Vec2) * NUM_VERTEX);

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

	glTranslatef(x, y, 0.0f);
	// make reasonable size
	glScalef(scale, scale, 1.0f);

	if (options & OPT_WIREFRAME) {
		glColor3ub(0xff, 0xff, 0);
	}
	texmgr.glbind(texture);

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
		angle += rotation * timestep;
		if (angle < 0.0f) {
			angle += 360.0f;
		}
		if (angle >= 360.0f) {
			angle -= 360.0f;
		}
	}
}

// draw a spinner
void Spinner::draw(void) {
	glPushMatrix();
	// center it
	glTranslatef(x, y, 0.0f);
	// make reasonable size
	glScalef(scale, scale, 1.0f);

	// spin it around
	glRotatef(angle, 0.0f, 0.0f, 1.0f);

	if (options & OPT_WIREFRAME) {
		glColor3ub(0, 0xa0, 0);
	} else {
		glColor3f(1.0f, 1.0f, 1.0f);
	}
	texmgr.glbind(TextureMgr::TEX_SMILEY);

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

void Scroller::move(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	if (options & OPT_PAUSED) {
		return;
	}
	x += direction * timestep * Scroller::SPEED;

	// when the scroller goes off-screen, restart it under an angle
	if ((x + Scroller::WIDTH + Scroller::WIDTH * 0.25f) < -SCREEN_WIDTH * 0.5f
		|| x > SCREEN_WIDTH * 0.5f + Scroller::WIDTH * 0.25f) {
		x = (GLfloat)SCREEN_WIDTH * 0.5f;
		angle = (GLfloat)(random() % 180 - 90);
		direction = -Scroller::SPEED;

		if (random() & 1) {
			direction = -direction;
			x = -x;
			x -= Scroller::WIDTH;
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
		glTranslatef(x, -(GLfloat)SCREEN_HEIGHT * 0.5f + Scroller::HEIGHT, 0.0f);
	}
	glScalef(Scroller::SCALE_X, Scroller::SCALE_Y, 1.0f);

	if (options & OPT_WIREFRAME) {
		glColor3ub(0xff, 0, 0xff);
	}
	// scroller text is an image texture
	// could have been TTF font rendering ...
	texmgr.glbind(texture);

	const GLfloat vertex_arr[8] = { 
		0, Scroller::HEIGHT,
		0, 0,
		Scroller::WIDTH, Scroller::HEIGHT,
		Scroller::WIDTH, 0
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
	x = random() % (SCREEN_WIDTH - Particle::WIDTH);
	y = SCREEN_HEIGHT + Particle::HEIGHT + (random() % SCREEN_HEIGHT);
	speed = (random() % 5) + 5.0f;
	drift = (random() % 5) * 0.5f - 1.0f;
	depth = random() % 6;
	speed = depth * 0.5f + 1.0f;	// speed based on depth
}

void Particle::move(float timestep) {
	if (depth != Particle::DEAD) {
		y -= speed * timestep * Particle::SPEED;
		speed += Particle::ACCEL;	// acceleration

		x += drift * timestep * Particle::SPEED;

		if (y <= -Particle::HEIGHT) {
			depth = Particle::DEAD;
		} else {
			if (x <= -Particle::WIDTH) {
				depth = Particle::DEAD;
			} else {
				if (x >= SCREEN_WIDTH) {
					depth = Particle::DEAD;
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
	vertex_arr[3] = y + Particle::HEIGHT;
	vertex_arr[4] = x + Particle::WIDTH;
	vertex_arr[5] = y;
	vertex_arr[6] = x + Particle::WIDTH;
	vertex_arr[7] = y + Particle::HEIGHT;

	glVertexPointer(2, GL_FLOAT, 0, vertex_arr);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ParticleSystem::init(void) {
	for(int n = 0; n < ParticleSystem::NUM_PARTICLES; n++) {
		parts[n].depth = Particle::DEAD;
	}
}

void ParticleSystem::move(float timestep) {
	if (timestep <= 0.001f) {
		return;
	}
	for(int n = 0; n < ParticleSystem::NUM_PARTICLES; n++) {
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
	texmgr.glbind(TextureMgr::TEX_BUBBLE);

	const GLfloat tex_arr[8] = {
		0, 0,
		0, 1,
		1, 0,
		1, 1
	};

	glTexCoordPointer(2, GL_FLOAT, 0, tex_arr);

	for(int n = 0; n < ParticleSystem::NUM_PARTICLES; n++) {
		if (parts[n].depth == depth		/* && visible */
			&& parts[n].y > -Particle::HEIGHT && parts[n].y < SCREEN_HEIGHT) {
			parts[n].draw();
		}
	}
	glDisable(GL_BLEND);

	glPopMatrix();
}

bool in_big_spinner(const Spinner& small) {
	// note: the big spinner is just plain 'spinner'
	// its radius equals scale (because it's unit * scale)
	float dx = spinner.x - small.x;
	float dy = spinner.y - small.y;
	float d_squared = dx * dx + dy * dy;
	float r = spinner.scale + small.scale;
	return d_squared <= r * r;
}

void draw_small_spinners(void) {
	int n = 0;
	float angle;

	small_spinner.y = SCREEN_HEIGHT * 0.2f;
	while(small_spinner.y <= SCREEN_HEIGHT * 0.8f) {
		small_spinner.x = SCREEN_WIDTH * 0.2f;
		while(small_spinner.x <= SCREEN_WIDTH * 0.8f) {
			if (!in_big_spinner(small_spinner)) {
				if (n == 36) {
					angle = small_spinner.angle;
					small_spinner.angle = 180.0f;
					small_spinner.draw();
					small_spinner.angle = angle;
				} else {
					small_spinner.draw();
				}
			}
			small_spinner.x += small_spinner.scale * 2.5f;
			n++;
		}
		small_spinner.y += small_spinner.scale * 2.5f;
	}
}

void draw_scene(void) {
	glClear(GL_COLOR_BUFFER_BIT);

	glLoadIdentity();
	// reverse camera
	glTranslatef(-cam_x, -cam_y, 0.0f);

	draw_small_spinners();
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

int main(int argc, const char *argv[]) {
	printf("glWHYz! demo " VERSION " - Copyright (C) 2007 2015 by Walter de Jong <walter@heiho.net>\n");

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init() failed\n");
		return -1;
	}
	create_window(SCREEN_WIDTH, SCREEN_HEIGHT);
	init_gl();

	if (!texmgr.load(sizeof(texture_filenames)/sizeof(const char **), texture_filenames)) {
		exit_program(-1);
	}
	wave.init();
	particles.init();

	small_spinner.scale = spinner.scale * 0.1f;
	small_spinner.rotation = -spinner.rotation;

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
			small_spinner.spin(timestep);
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
