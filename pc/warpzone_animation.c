// Using legacy OpenGL to render our animated logo
// ands

#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <GL/gl.h>
#include <SDL/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// avoids depending on glext.h for just these two defines
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

// texture enumeration
#define RADIAL0			0
#define RADIAL1			1
#define RADIAL2			2
#define ARROW			3
#define EVENTS			4
#define TEXT			5
#define TEXTURES_NUM	6

const char *texture_files[] =
{
	"radial0.png",
	"radial1.png",
	"radial2.png",
	"arrow.png",
	"events.png",
	"text.png"
};

GLuint textures[TEXTURES_NUM] = { 0 };

static void draw_quad(float size)
{
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-size,  size);
		glTexCoord2f(1.0f, 0.0f); glVertex2f( size,  size);
		glTexCoord2f(1.0f, 1.0f); glVertex2f( size, -size);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-size, -size);
	glEnd();
}

static void draw_logo(double time)
{
	// draw radials
	for (int i = 0; i < 3; i++)
	{
		glPushMatrix();
			glRotatef(20.0f * cos(time * 2.0f), 0.0f, 1.0f, 0.0f);
			glTranslatef(0.0f, 0.8f, 0.0f);
			glRotatef(time * 32.0f * (0.5f + (i - 1)) , 0.0f, 0.0f, 1.0f);
			glBindTexture(GL_TEXTURE_2D, textures[RADIAL0 + i]);
			draw_quad(1.0f);
		glPopMatrix();
	}

	// draw text
	glPushMatrix();
		glRotatef(60.0f * sin(time), 0.0f, 1.0f, 0.0f);
		glTranslatef(0.0f, -0.8f, 0.0f);
		glBindTexture(GL_TEXTURE_2D, textures[TEXT]);
		draw_quad(1.0f);
	glPopMatrix();
}

static void draw_events(double time)
{
	// draw events
	glPushMatrix();
		glRotatef(30.0f * sin(time), 0.0f, 1.0f, 0.0f);
		glBindTexture(GL_TEXTURE_2D, textures[EVENTS]);
		draw_quad(1.0f);
	glPopMatrix();
	
	// draw arrows
	glPushMatrix();
		glTranslatef(0.0f, -1.5f, 0.0f);
		glRotatef(-50.0f + 20.0f * cos(time * 2.0f), 0.0f, 1.0f, 0.0f);
		glBindTexture(GL_TEXTURE_2D, textures[ARROW]);
		draw_quad(0.4f);
	glPopMatrix();
	glPushMatrix();
		glTranslatef(0.0f, 1.5f, 0.0f);
		glRotatef(50.0f + 20.0f * cos(time * 2.5f), 0.0f, 1.0f, 0.0f);
		glBindTexture(GL_TEXTURE_2D, textures[ARROW]);
		draw_quad(0.4f);
	glPopMatrix();
}

#define SWITCH_INTERVAL 10.0 // seconds

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <events|noevents>", argv[0]);
		exit(-1);
	}
	int show_events = argv[1][0] != 'n'; // lazy hack

	// initialize window + opengl context
	SDL_Init(SDL_INIT_VIDEO);
	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	SDL_SetVideoMode(info->current_w, info->current_h, 0, SDL_OPENGL | SDL_FULLSCREEN);
	SDL_ShowCursor(SDL_DISABLE);
	
	// initial opengl state
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	// camera projection setup
	glMatrixMode(GL_PROJECTION);
	const double fov = 50.0, aspect = info->current_w / (double)info->current_h, near = 0.1, far = 1000.0;
	const double half_height = tan(fov / 360.0 * 3.14159) * near;
	const double half_width = half_height * aspect;
	glFrustum(-half_width, half_width, -half_height, half_height, near, far);
	glMatrixMode(GL_MODELVIEW);

	// load textures
	GLfloat max_anisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
	
	glGenTextures(TEXTURES_NUM, textures);
	for (int i = 0; i < TEXTURES_NUM; i++)
	{
		int x, y, n;
		unsigned char *data = stbi_load(texture_files[i], &x, &y, &n, 3);
		if (data == NULL)
		{
			fprintf(stderr, "Could not load file: %s", texture_files[i]);
			exit(-1);
		}
		
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		
		stbi_image_free(data);
	}
	
	// rendering loop
	SDL_Event event;
	do
	{
		// get current time
		double time = SDL_GetTicks() / 1000.0;
		
		// clear output buffer
		glClear(GL_COLOR_BUFFER_BIT /*| GL_DEPTH_BUFFER_BIT*/);
		
		glLoadIdentity();
		glTranslatef(0.0f, 0.0f, -3.0f); // camera positioning
		glRotatef(-90.0f, 0.0f, 0.0f, 1.0f); // beamer rotation
		glRotatef(180.0f, 0.0f, 1.0f, 0.0f); // horizontal "flip"
		
		if (!show_events || (int)(time/SWITCH_INTERVAL) & 1)
			draw_logo(time);
		else
			draw_events(time);
		
		// flush buffer and wait for next frame
		SDL_GL_SwapBuffers();
		usleep(16000); // save cpu time (~60FPS)
		SDL_PollEvent(&event);
	} while(event.type != SDL_KEYDOWN);
	
	// clean up
	glDeleteTextures(TEXTURES_NUM, textures);
	SDL_Quit();
}
