// canvas.h - the 32-bit picture the menus are drawn into (400 x 300, shown 4:3 by the
// presenter at any window size) and the 8x8 text on it. Moved out of main.cpp for the menu
// toolkit (roadmap U1); no SDL in here.
#pragma once
#include "../font.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

struct canvas
{
	int w, h;
	std::vector<uint32_t> px;
	canvas(int W, int H) : w(W), h(H), px(size_t(W) * H, 0xff000000) {}
	void fill(int x, int y, int ww, int hh, uint32_t c)
	{
		for (int j = std::max(0, y); j < std::min(h, y + hh); j++)
			for (int i = std::max(0, x); i < std::min(w, x + ww); i++) px[size_t(j) * w + i] = c;
	}
	void blend(int x, int y, int ww, int hh, uint32_t c, int alpha)
	{
		for (int j = std::max(0, y); j < std::min(h, y + hh); j++)
			for (int i = std::max(0, x); i < std::min(w, x + ww); i++)
			{
				uint32_t &d = px[size_t(j) * w + i];
				auto mix = [&](int sh) { return uint32_t((((d >> sh) & 0xff) * (255 - alpha) + ((c >> sh) & 0xff) * alpha) / 255) << sh; };
				d = 0xff000000 | mix(16) | mix(8) | mix(0);
			}
	}
	// outline of a rectangle, one pixel thick
	void frame(int x, int y, int ww, int hh, uint32_t c)
	{
		fill(x, y, ww, 1, c); fill(x, y + hh - 1, ww, 1, c);
		fill(x, y, 1, hh, c); fill(x + ww - 1, y, 1, hh, c);
	}
	void text(int x, int y, const std::string &s, uint32_t c, int scale = 1, bool shadow = true)
	{
		if (shadow) text(x + scale, y + scale, s, 0xff000000, scale, false);
		for (size_t n = 0; n < s.size(); n++)
		{
			unsigned ch = static_cast<unsigned char>(s[n]);
			if (ch > 127) ch = '?';
			for (int row = 0; row < 8; row++)
			{
				unsigned bits = g_font8[ch][row];
				for (int col = 0; col < 8; col++)
					if (bits & (1u << col))
						fill(x + int(n) * 8 * scale + col * scale, y + row * scale, scale, scale, c);
			}
		}
	}
	int text_w(const std::string &s, int scale = 1) const { return int(s.size()) * 8 * scale; }
	// word-wrapped text that never leaves the canvas; returns the y below the last line
	int text_wrap(int x, int y, const std::string &s, uint32_t c, int line_h = 10, bool shadow = false)
	{
		size_t maxc = size_t(std::max(1, (w - x - 2) / 8));
		std::string rest = s;
		while (!rest.empty())
		{
			std::string ln = rest;
			if (ln.size() > maxc)
			{
				size_t cut = rest.rfind(' ', maxc);
				if (cut == std::string::npos || cut == 0) cut = maxc;
				ln = rest.substr(0, cut);
				rest = rest.substr(cut);
				while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
			}
			else rest.clear();
			text(x, y, ln, c, 1, shadow);
			y += line_h;
		}
		return y;
	}
};
