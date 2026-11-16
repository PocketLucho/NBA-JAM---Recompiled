// art_view.cpp - see art_view.h
#include "art_view.h"
#include <algorithm>

namespace ui
{

void draw_picture(canvas &cv, const rom_art::picture &p, int x, int y, int shrink)
{
	shrink = std::max(1, shrink);
	for (int py = 0; py < p.h; py += shrink)
		for (int px = 0; px < p.w; px += shrink)
		{
			uint32_t c = p.at(px, py);
			int dx = x + px / shrink, dy = y + py / shrink;
			if ((c >> 24) && dx >= 0 && dy >= 0 && dx < cv.w && dy < cv.h) cv.px[size_t(dy) * cv.w + dx] = c;
		}
}

int draw_rom_text(canvas &cv, const rom_art::font &f, int x, int y, const std::string &s, int max_x)
{
	for (char ch : s)
	{
		if (ch == ' ') { x += std::max(2, f.cap_h / 3); continue; }
		if (!f.has(ch)) continue;
		const rom_art::picture &g = f.glyph[static_cast<unsigned char>(ch)];
		if (x + g.w > max_x) break;
		draw_picture(cv, g, x, y + std::max(0, f.cap_h - g.h));     // letters stand on one line
		x += g.w + 1;
	}
	return x;
}

int art_pages(const rom_art::art_set &s)
{
	if (s.empty()) return 1;
	return (s.fonts.empty() ? 0 : 1) + (s.logos.empty() ? 0 : 1) + int(s.backgrounds.size());
}

void draw_art_page(canvas &cv, const theme &th, const std::string &game, const rom_art::art_set &s, int page, device dev)
{
	auto note = [&](int x, int y, const std::string &t, uint32_t c) { draw_text(cv, x, y, fit_px(t, cv.w - x - 8, face::note), face::note, th.ink_for(c)); };
	for (int y = 0; y < cv.h; y++)
	{
		auto mix = [&](int sh) { int a = (th.bg_top >> sh) & 0xff, b = (th.bg_bottom >> sh) & 0xff; return uint32_t(a + (b - a) * y / std::max(1, cv.h - 1)) << sh; };
		cv.fill(0, y, cv.w, 1, 0xff000000 | mix(16) | mix(8) | mix(0));
	}
	const int pages = art_pages(s);
	page = std::clamp(page, 0, pages - 1);
	static const char *sample[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789 !?.,:'-" };
	std::string what;
	int first_bg = (s.fonts.empty() ? 0 : 1) + (s.logos.empty() ? 0 : 1);

	if (s.empty())
	{
		// the fallback: the project's own lettering, as the menus will use it
		what = "No ROM art for this game: the project lettering is used";
		int y = 44;
		for (face f : { face::title, face::list, face::note })
			for (const char *l : sample)
			{
				draw_text(cv, 8, y, fit_px(l, cv.w - 16, f), f, f == face::title ? ink{ th.title_top, th.title_bottom, th.outline } : th.ink_for(th.text));
				y += text_h(f) + 6;
			}
	}
	else if (page >= first_bg)
	{
		const rom_art::picture &bg = s.backgrounds[size_t(page - first_bg)];
		what = "Background " + std::to_string(page - first_bg + 1) + " of " + std::to_string(s.backgrounds.size()) + " (" + std::to_string(bg.w) + " x " + std::to_string(bg.h) + ")";
		draw_picture(cv, bg, (cv.w - bg.w) / 2, 24);
	}
	else if (page == 0 && !s.fonts.empty())
	{
		what = "Lettering read from the ROM: " + std::to_string(s.fonts.size()) + (s.fonts.size() == 1 ? " font" : " fonts");
		int y = 40;
		for (const rom_art::font &f : s.fonts)
		{
			if (y + f.cap_h > 280) break;
			int x = draw_rom_text(cv, f, 8, y, sample[0], cv.w - 8);
			draw_rom_text(cv, f, x + std::max(3, f.cap_h / 2), y, sample[1], cv.w - 8);
			y += f.cap_h + 5;
		}
	}
	else
	{
		what = "Team logos read from the ROM: " + std::to_string(s.logos.size()) + ", shown at half size";
		int x = 8, y = 40, row_h = 0;
		for (const rom_art::picture &l : s.logos)
		{
			int w = (l.w + 1) / 2, h = (l.h + 1) / 2;
			if (x + w > cv.w - 8) { x = 8; y += row_h + 6; row_h = 0; }
			draw_picture(cv, l, x, y, 2);
			x += w + 6; row_h = std::max(row_h, h);
		}
	}

	std::string ttl = fit_px(game, cv.w - 16, face::list);
	draw_text(cv, 8, 3, ttl, face::list, { th.title_top, th.title_bottom, th.outline });
	note(8, 14, what, th.text);
	std::string st = "[Left/Right] Game  [Up/Down] Page " + std::to_string(page + 1) + "/" + std::to_string(pages) + "  [" + glyph(dev, act::back) + "] Back";
	note(8, cv.h - 11, st, th.status);
}

} // namespace ui
