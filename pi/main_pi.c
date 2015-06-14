#include <stdio.h>
#include <math.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "r3d_math.h"

// avoids depending on glext.h for just these two defines
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

static EGLDisplay display;
static EGLSurface surface;
static EGLConfig config;
static EGLContext context;
static DISPMANX_DISPLAY_HANDLE_T dispman_display;
static DISPMANX_UPDATE_HANDLE_T dispman_update;
static DISPMANX_ELEMENT_HANDLE_T dispman_element;
static EGL_DISPMANX_WINDOW_T nativewindow;
static VC_RECT_T dst_rect;
static VC_RECT_T src_rect;
static void LoadOpenGLESWindow(uint32_t width, uint32_t height)
{
    bcm_host_init();
    
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, NULL, NULL);
    
    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_NONE
	};
    EGLint num_config;
    eglChooseConfig(display, attribute_list, &config, 1, &num_config);
    
    eglBindAPI(EGL_OPENGL_ES_API);

    static const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = width;
    dst_rect.height = height;
    
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = width << 16;
    src_rect.height = height << 16;

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );
    dispman_element = vc_dispmanx_element_add(
    	dispman_update, dispman_display,
		0/*layer*/, &dst_rect, 0/*src*/,
		&src_rect, DISPMANX_PROTECTION_NONE, 
		0 /*alpha*/, 0/*clamp*/, 0/*transform*/);
    nativewindow.element = dispman_element;
    nativewindow.width = width;
    nativewindow.height = height;
    vc_dispmanx_update_submit_sync(dispman_update);

    surface = eglCreateWindowSurface(display, config, &nativewindow, NULL);
    eglMakeCurrent(display, surface, surface, context);
}

static GLuint LoadShader(GLenum type, const char *shaderSrc)
{
    GLuint shader = glCreateShader(type);
    if(shader == 0) return 0;
    glShaderSource(shader, 1, &shaderSrc, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if(!compiled)
    {
	    GLint infoLen = 0;
	    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
	    if(infoLen > 1)
		{
		    char* infoLog = malloc(sizeof(char) * infoLen);
		    glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
		    fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
		    free(infoLog);
		}
	    glDeleteShader(shader);
	    return 0;
	}
    return shader;
}

static GLuint LoadProgram(const char *vp, const char *fp)
{
    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vp);
    GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fp);
    GLuint programObject = glCreateProgram();
    if(programObject == 0) return 0;
    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    glBindAttribLocation(programObject, 0, "vPosition");
    glBindAttribLocation(programObject, 1, "vTexcoord");
    glLinkProgram(programObject);
    GLint linked;
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if(!linked)
	{
	    GLint infoLen = 0;
	    glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
	    if(infoLen > 1)
		{
		    char* infoLog = malloc(sizeof(char) * infoLen);
		    glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
		    fprintf(stderr, "Error linking program:\n%s\n", infoLog);
		    free(infoLog);
		}
	    glDeleteProgram(programObject);
	    return 0;
	}
	return programObject;
}

// texture enumeration
#define RADIAL0			0
#define RADIAL1			1
#define RADIAL2			2
#define ARROW			3
#define EVENTS			4
#define TEXT			5
#define TEXTURES_NUM	6

static const char *texture_files[] =
{
	"radial0.png",
	"radial1.png",
	"radial2.png",
	"arrow.png",
	"events.png",
	"text.png"
};

static const GLfloat quad_vertices[] =
{
	-1.0f,  1.0f,
	 1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f, -1.0f,
	-1.0f, -1.0f,
	 1.0f,  1.0f
};

static const GLfloat quad_texcoord[] =
{
	0.0f, 0.0f,
	1.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 1.0f,
	1.0f, 0.0f
};

static const char vertex_shader[] =
	"attribute vec2 vPosition;\n"
	"attribute vec2 vTexcoord;\n"
	"varying vec2 uv;\n"
	"uniform mat4 transform;\n"
	"void main()\n"
	"{\n"
	    "gl_Position = transform * vec4(vPosition, 0, 1);\n"
	    "uv = vTexcoord;\n"
	"}\n";
static const char fragment_shader[] =
	"precision mediump float;\n"
	"varying vec2 uv;\n"
	"uniform sampler2D tex;\n"
	"uniform vec3 color;\n"
	"void main()\n"
	"{\n"
	    "gl_FragColor = vec4(color * texture2D(tex, uv).rgb, 1); \n"
	"}\n";

int main(int argc, char *argv[])
{
	uint32_t width = 1920, height = 1080;
	LoadOpenGLESWindow(width, height);
    
	// initial opengl state
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_CULL_FACE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	// load textures
	GLfloat max_anisotropy;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
	GLuint textures[TEXTURES_NUM];
	glGenTextures(TEXTURES_NUM, textures);
	int i;
	for (i = 0; i < TEXTURES_NUM; i++)
	{
		int x, y, n;
		unsigned char *data = stbi_load(texture_files[i], &x, &y, &n, 3);
		if (data == NULL)
		{
			fprintf(stderr, "Could not load file: %s", texture_files[i]);
			exit(-1);
		}
		
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
		
		stbi_image_free(data);
	}
	
	// load program
	GLuint programObject = LoadProgram(vertex_shader, fragment_shader);
	glUseProgram(programObject);
	glUniform1i(glGetUniformLocation(programObject, "tex"), 0);
	GLint colorUniformLocation = glGetUniformLocation(programObject, "color");
	GLint transformUniformLocation = glGetUniformLocation(programObject, "transform");
	
	// load quad data
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad_vertices);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoord);
	glEnableVertexAttribArray(1);

    unsigned int frames = 0;
    while(1)
    {
		glViewport(0, 0, width, height);
		
		// animation params
		double t = (frames++) * 0.02f;
		float a = 0.5f + 0.5f * sinf(t * 13.0f);
		float x = 0.2f * sinf(32.0f * t) * sinf(3.5f * t);
		float y = 0.1f * sinf(23.0f * t) * sinf(3.5f * t);
		
		// camera config
		mat4_t proj = mat4_perspective(50.0f, (float)width / (float)height, 0.1f, 1000.0f);
		mat4_t view = mat4_mul(//mat4_mul(
			mat4_translation(vec3(x, y, -3.0f)), // positioning
			mat4_rotation(-90.0f, vec3(0.0f, 0.0f, 1.0f)));//, // beamer rotation
			//mat4_rotation(180.0f, vec3(0.0f, 1.0f, 0.0f))); // horizontal "flip"
		mat4_t vp = mat4_mul(proj, view);
		
		// color
		glUniform3f(colorUniformLocation, 
			a * (0.5f + 0.5f * sinf(t)),
			a * (0.5f + 0.5f * sinf(1.3f * t)),
			a * (0.5f + 0.5f * sinf(1.7f * t)));
			
		// draw radials
		int i;
		for (i = 0; i < 3; i++)
		{
			mat4_t mvp = mat4_mul(vp, mat4_rotation(20.0f * cos(t * 2.0f), vec3(0.0f, 1.0f, 0.0f)));
			mvp = mat4_mul(mvp, mat4_translation(vec3(0.0f, 0.8f, 0.0f)));
			mvp = mat4_mul(mvp, mat4_rotation(t * 32.0f * (0.5f + (i - 1)), vec3(0.0f, 0.0f, 1.0f)));
			glUniformMatrix4fv(transformUniformLocation, 1, GL_FALSE, mvp.m);
			glBindTexture(GL_TEXTURE_2D, textures[RADIAL0 + i]);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		// draw text
		mat4_t mvp = mat4_mul(vp, mat4_rotation(60.0f * sin(t), vec3(0.0f, 1.0f, 0.0f)));
		mvp = mat4_mul(mvp, mat4_translation(vec3(0.0f, -0.8f, 0.0f)));
		glUniformMatrix4fv(transformUniformLocation, 1, GL_FALSE, mvp.m);
		glBindTexture(GL_TEXTURE_2D, textures[TEXT]);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		// present
		eglSwapBuffers(display, surface);
    }
}
