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
	// side_fill: what is shown beside a picture narrower than the window (W1.5):
	// 0 black, 1 the picture blurred and dimmed, 2 the overlay image (black if none is set)
	void present(const uint32_t *px, int w, int h, int shader, const view_rect &view, float scan_strength, bool swap = true, int side_fill = 0);
	// Overlay / border image, RGBA, top row first (side_fill 2). Like a RetroArch border: it
	// is scaled to the height of the window, keeping its shape, and centred, so a 16:9 border
	// with a 4:3 opening lines up with a picture that fills the height on any monitor.
	// An image with transparency is drawn over the game (the game shows through the opening);
	// a fully opaque one is drawn behind it. Null removes it.
	void set_overlay(const uint8_t *rgba, int w, int h);
	bool has_overlay() const { return m_bezel != 0; }
	void set_overlay_opacity(float o) { m_ov_opacity = o; }
	// where the overlay goes in a window of that size (for tests)
	static void overlay_rect(int win_w, int win_h, int img_w, int img_h, int &x, int &w);
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
	unsigned m_bezel = 0, m_blur = 0, m_ovprog = 0;
	int m_ov_w = 0, m_ov_h = 0;
	bool m_ov_alpha = false;
	float m_ov_opacity = 1.0f;
	void draw_overlay(int ww, int wh);
	unsigned m_prog[8] = {};
	int m_nprog = 0;
};

