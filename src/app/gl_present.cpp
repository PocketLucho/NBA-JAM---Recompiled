// gl_present.cpp - see gl_present.h
#include "gl_present.h"
#include "shaders.h"
#include <algorithm>
#include <vector>

// minimal GL definitions (no system GL headers needed)
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef void GLvoid;
#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_FLOAT 0x1406
#define GL_TRIANGLE_STRIP 0x0005
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_TEXTURE0 0x84C0

#define GLFUNCS \
	F(void, glGenTextures, (GLsizei, GLuint *)) \
	F(void, glDeleteTextures, (GLsizei, const GLuint *)) \
	F(void, glBindTexture, (GLenum, GLuint)) \
	F(void, glTexParameteri, (GLenum, GLenum, GLint)) \
	F(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *)) \
	F(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *)) \
	F(void, glPixelStorei, (GLenum, GLint)) \
	F(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
	F(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
	F(void, glClear, (GLenum)) \
	F(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
	F(GLenum, glGetError, (void)) \
	F(GLuint, glCreateShader, (GLenum)) \
	F(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
	F(void, glCompileShader, (GLuint)) \
	F(void, glGetShaderiv, (GLuint, GLenum, GLint *)) \
	F(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	F(void, glDeleteShader, (GLuint)) \
	F(GLuint, glCreateProgram, (void)) \
	F(void, glAttachShader, (GLuint, GLuint)) \
	F(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *)) \
	F(void, glLinkProgram, (GLuint)) \
	F(void, glGetProgramiv, (GLuint, GLenum, GLint *)) \
	F(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	F(void, glDeleteProgram, (GLuint)) \
	F(void, glUseProgram, (GLuint)) \
	F(GLint, glGetUniformLocation, (GLuint, const GLchar *)) \
	F(void, glUniform1i, (GLint, GLint)) \
	F(void, glUniform1f, (GLint, GLfloat)) \
	F(void, glUniform2f, (GLint, GLfloat, GLfloat)) \
	F(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *)) \
	F(void, glEnableVertexAttribArray, (GLuint)) \
	F(void, glActiveTexture, (GLenum))

#define F(ret, name, args) typedef ret (APIENTRY *PFN_##name) args; static PFN_##name p_##name;
GLFUNCS
#undef F

static bool load_gl(std::string &err)
{
#define F(ret, name, args) p_##name = (PFN_##name)SDL_GL_GetProcAddress(#name); if (!p_##name) { err = "falta la funcion OpenGL " #name; return false; }
	GLFUNCS
#undef F
	return true;
}

void output_rect(int ww, int wh, int w, int h, bool integer, float aspect, int &x, int &y, int &dw, int &dh)
{
	if (aspect <= 0)
	{
		dw = ww; dh = wh;
		if (integer && h > 0) dh = std::max(1, wh / h) * h;
	}
	else
	{
		dw = ww; dh = int(ww / aspect);
		if (dh > wh) { dh = wh; dw = int(wh * aspect); }
		if (integer && h > 0)
		{
			int n = std::max(1, dh / h);
			dh = n * h;
			dw = std::min(ww, int(dh * aspect));
		}
	}
	x = (ww - dw) / 2;
	y = (wh - dh) / 2;
}

static GLuint compile(GLenum type, const std::string &src, std::string &err)
{
	GLuint s = p_glCreateShader(type);
	const char *c = src.c_str();
	p_glShaderSource(s, 1, &c, nullptr);
	p_glCompileShader(s);
	GLint ok = 0;
	p_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[2048] = {};
		p_glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
		err = log;
		p_glDeleteShader(s);
		return 0;
	}
	return s;
}

bool gl_presenter::init(SDL_Window *win, std::string &err)
{
	m_win = win;
	m_ctx = SDL_GL_CreateContext(win);
	if (!m_ctx) { err = SDL_GetError(); return false; }
	SDL_GL_MakeCurrent(win, m_ctx);
	if (!load_gl(err)) { shutdown(); return false; }
	SDL_GL_SetSwapInterval(0);      // the game paces itself (~54.7 Hz)

	// desktop GL 2.1 takes GLSL 1.10; GLES/ANGLE contexts take GLSL ES 1.00
	const char *versions[2] = { "#version 110\n", "#version 100\n" };
	int vi = 0;
	for (int i = 0; i < kNumShaders; i++)
	{
		std::string e;
		GLuint v = 0, f = 0;
		for (; vi < 2; vi++)
		{
			v = compile(GL_VERTEX_SHADER, std::string(versions[vi]) + kVertexShader, e);
			f = v ? compile(GL_FRAGMENT_SHADER, std::string(versions[vi]) + kFragHeader + kShaders[i].src, e) : 0;
			if (v && f) break;
			if (v) p_glDeleteShader(v);
			v = 0;
		}
		if (!v || !f) { err = std::string("shader '") + kShaders[i].name + "': " + e; shutdown(); return false; }
		GLuint p = p_glCreateProgram();
		p_glAttachShader(p, v);
		p_glAttachShader(p, f);
		p_glBindAttribLocation(p, 0, "a_pos");
		p_glBindAttribLocation(p, 1, "a_uv");
		p_glLinkProgram(p);
		p_glDeleteShader(v);
		p_glDeleteShader(f);
		GLint ok = 0;
		p_glGetProgramiv(p, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			char log[2048] = {};
			p_glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
			err = std::string("shader '") + kShaders[i].name + "': " + log;
			p_glDeleteProgram(p);
			shutdown();
			return false;
		}
		m_prog[m_nprog++] = p;
	}
	p_glGenTextures(1, &m_tex);
	m_ok = true;
	return true;
}

void gl_presenter::shutdown()
{
	if (m_ctx)
	{
		if (p_glDeleteProgram) for (int i = 0; i < m_nprog; i++) p_glDeleteProgram(m_prog[i]);
		if (m_tex && p_glDeleteTextures) p_glDeleteTextures(1, &m_tex);
		SDL_GL_DeleteContext(m_ctx);
	}
	m_ctx = nullptr;
	m_nprog = 0;
	m_tex = 0;
	m_tw = m_th = 0;
	m_ok = false;
}

void gl_presenter::present(const uint32_t *px, int w, int h, int shader, bool integer, float scan_strength, float aspect)
{
	if (!m_ok) return;
	shader = std::clamp(shader, 0, m_nprog - 1);
	p_glActiveTexture(GL_TEXTURE0);
	p_glBindTexture(GL_TEXTURE_2D, m_tex);
	p_glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	// ARGB8888 -> RGBA bytes (GL_BGRA isn't available on GLES/ANGLE contexts)
	static std::vector<uint32_t> conv;
	conv.resize(size_t(w) * h);
	for (size_t i = 0; i < conv.size(); i++)
	{
		uint32_t v = px[i];
		conv[i] = 0xff000000u | (v & 0x0000ff00u) | ((v >> 16) & 0xffu) | ((v & 0xffu) << 16);
	}
	if (w != m_tw || h != m_th)
	{
		p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv.data());
		m_tw = w; m_th = h;
	}
	else
		p_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, conv.data());

	int ww, wh;
	SDL_GL_GetDrawableSize(m_win, &ww, &wh);
	int x, y, dw, dh;
	output_rect(ww, wh, w, h, integer, aspect, x, y, dw, dh);
	p_glViewport(0, 0, ww, wh);
	p_glClearColor(0, 0, 0, 1);
	p_glClear(GL_COLOR_BUFFER_BIT);
	p_glViewport(x, wh - y - dh, dw, dh);

	GLuint p = m_prog[shader];
	p_glUseProgram(p);
	p_glUniform1i(p_glGetUniformLocation(p, "u_tex"), 0);
	p_glUniform2f(p_glGetUniformLocation(p, "u_src"), float(w), float(h));
	p_glUniform2f(p_glGetUniformLocation(p, "u_out"), float(dw), float(dh));
	float opt = shader == 2 ? scan_strength : shader == 4 ? 1.0f : 0.0f;
	p_glUniform1f(p_glGetUniformLocation(p, "u_opt"), opt);
	static const float quad[] = { -1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0 };
	p_glVertexAttribPointer(0, 2, GL_FLOAT, 0, 16, quad);
	p_glVertexAttribPointer(1, 2, GL_FLOAT, 0, 16, quad + 2);
	p_glEnableVertexAttribArray(0);
	p_glEnableVertexAttribArray(1);
	p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	SDL_GL_SwapWindow(m_win);
}
