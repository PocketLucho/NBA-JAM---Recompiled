// gl_present.h - OpenGL 2.1 output with selectable shaders (CRT, scanlines, sharp...)
#pragma once
#include <SDL.h>
#include <cstdint>
#include <string>
#include <vector>
#include "view.h"

class gl_presenter
{
public:
	bool init(SDL_Window *win, std::string &err);   // window must have SDL_WINDOW_OPENGL
	void shutdown();
	bool ok() const { return m_ok; }
	// draws pixels (ARGB8888) into the rectangle chosen by compute_view (view.h).
	// swap = false leaves the frame in the back buffer, for capture().
	void present(const uint32_t *px, int w, int h, int shader, const view_rect &view, float scan_strength, bool swap = true);
	// the frame just drawn with swap = false, as RGB bytes, top row first (tools/headless/viewtest)
	bool capture(std::vector<uint8_t> &rgb, int &w, int &h);
	void swap();
	void drawable_size(int &w, int &h) const;
private:
	bool m_ok = false;
	SDL_Window *m_win = nullptr;
	SDL_GLContext m_ctx = nullptr;
	unsigned m_tex = 0;
	int m_tw = 0, m_th = 0;
	unsigned m_prog[8] = {};
	int m_nprog = 0;
};

