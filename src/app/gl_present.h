// gl_present.h - OpenGL 2.1 output with selectable shaders (CRT, scanlines, sharp...)
#pragma once
#include <SDL.h>
#include <cstdint>
#include <string>

class gl_presenter
{
public:
	bool init(SDL_Window *win, std::string &err);   // window must have SDL_WINDOW_OPENGL
	void shutdown();
	bool ok() const { return m_ok; }
	// draws pixels (ARGB8888) into a 4:3 area; integer = vertical scale is a whole number
	void present(const uint32_t *px, int w, int h, int shader, bool integer, float scan_strength, float aspect);
private:
	bool m_ok = false;
	SDL_Window *m_win = nullptr;
	SDL_GLContext m_ctx = nullptr;
	unsigned m_tex = 0;
	int m_tw = 0, m_th = 0;
	unsigned m_prog[8] = {};
	int m_nprog = 0;
};

// computes the destination rectangle (shared with the software fallback)
// aspect: display aspect ratio (4/3 arcade, 16/9 widescreen), 0 = fill the window
void output_rect(int ww, int wh, int w, int h, bool integer, float aspect, int &x, int &y, int &dw, int &dh);
