// themes.cpp - see themes.h
#include "themes.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ui
{

// ---------------------------------------------------------------- the built-in themes
// Colours are this project's own choices. The background and font lines only say which of
// the user's ROM pictures to use.
static const struct { const char *id, *text; } kBuiltin[] = {
	{ "neutral",
	  "name = Neutral\n"
	  "title_top = #ffec60\ntitle_bottom = #ff5a10\noutline = #000000\n"
	  "bg_top = #14102c\nbg_bottom = #301c5c\n"
	  "tab = #b0b0d0\ntab_sel = #ffffff\n"
	  "panel = #1c1638\npanel_edge = #5a4a9a\nstrip = #3a2a78\nstrip_text = #ffe080\n"
	  "text = #c8d8ff\ntext_focus = #ffffff\ntext_off = #707088\nheader = #80ff80\n"
	  "bar = #ffa020\nvalue = #ffe080\nstatus = #b0b0b0\ndialog = #241a50\n" },
	{ "nbajam",
	  "name = NBA JAM\ngames = nbajam\n"
	  "title_top = #fff070\ntitle_bottom = #ff4a10\noutline = #000000\n"
	  "bg_top = #0c1438\nbg_bottom = #2c1c70\n"
	  "tab = #a8b8e8\ntab_sel = #ffffff\n"
	  "panel = #101840\npanel_edge = #4868c8\nstrip = #2c3c98\nstrip_text = #ffe080\n"
	  "text = #d0dcff\ntext_focus = #ffffff\ntext_off = #68708c\nheader = #60e0ff\n"
	  "bar = #ff9020\nvalue = #ffe080\nstatus = #c0c0c8\ndialog = #182058\n"
	  "panel_alpha = 225\n"
	  "background = 398x254\nbackground_crop = 12 196 84 50\nbackground_dim = 120\n"
	  "title_font = 13\nlist_font = 0\n" },
	{ "te",
	  "name = NBA JAM Tournament Edition\ngames = nbajamte nbajamse\n"
	  "title_top = #ffe870\ntitle_bottom = #c87800\noutline = #000000\n"
	  "bg_top = #080c20\nbg_bottom = #182850\n"
	  "tab = #a0b0d8\ntab_sel = #ffffff\n"
	  "panel = #0c1228\npanel_edge = #6878a8\nstrip = #283868\nstrip_text = #ffd858\n"
	  "text = #c8d4f0\ntext_focus = #ffffff\ntext_off = #606880\nheader = #ffd040\n"
	  "bar = #e8a000\nvalue = #ffd858\nstatus = #b8c0d0\ndialog = #141c40\n"
	  "panel_alpha = 225\n"
	  "background = 398x254\nbackground_crop = 12 196 84 50\nbackground_dim = 150\n"
	  "title_font = 13\nlist_font = 0\n" },
	{ "hangtime",
	  "name = NBA Hangtime\ngames = nbahangt\n"
	  "title_top = #fff080\ntitle_bottom = #ff9000\noutline = #000000\n"
	  "bg_top = #180830\nbg_bottom = #400818\n"
	  "tab = #b8a8e0\ntab_sel = #ffffff\n"
	  "panel = #140a28\npanel_edge = #5038a0\nstrip = #301870\nstrip_text = #ffe060\n"
	  "text = #e0d8ff\ntext_focus = #ffffff\ntext_off = #6c6484\nheader = #50a8ff\n"
	  "bar = #e02828\nvalue = #ffe060\nstatus = #c0b8d0\ndialog = #1c1040\n"
	  "panel_alpha = 205\n"
	  "background = 400x254\nbackground_dim = 40\n"
	  "title_font = 18\nlist_font = 9\n" },
	{ "mht",
	  "name = NBA Maximum Hangtime\ngames = nbamht\n"
	  "title_top = #fff8a0\ntitle_bottom = #f0a000\noutline = #000000\n"
	  "bg_top = #200818\nbg_bottom = #100838\n"
	  "tab = #c8b0c8\ntab_sel = #ffffff\n"
	  "panel = #180a20\npanel_edge = #a03048\nstrip = #581830\nstrip_text = #ffe060\n"
	  "text = #f0e0e8\ntext_focus = #ffffff\ntext_off = #74606c\nheader = #ff5050\n"
	  "bar = #f0c000\nvalue = #ffe060\nstatus = #d0b8c0\ndialog = #24102c\n"
	  "panel_alpha = 205\n"
	  "background = 400x254\nbackground_dim = 40\n"
	  "title_font = 18\nlist_font = 9\n" },
};

const std::vector<std::string> &theme_ids()
{
	static const std::vector<std::string> ids = [] { std::vector<std::string> v; for (auto &b : kBuiltin) v.push_back(b.id); return v; }();
	return ids;
}

const char *builtin_theme(const std::string &id)
{
	for (auto &b : kBuiltin) if (id == b.id) return b.text;
	return nullptr;
}

std::string theme_id_for_game(const std::string &game_id)
{
	for (auto &b : kBuiltin)
	{
		theme_spec s = parse_theme(b.id, b.text);
		if (std::find(s.games.begin(), s.games.end(), game_id) != s.games.end()) return b.id;
	}
	return "neutral";
}

// ---------------------------------------------------------------- reading a theme file
static std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static bool colour(const std::string &v, uint32_t &out)
{
	if (v.size() != 7 || v[0] != '#') return false;
	char *end = nullptr;
	unsigned long n = strtoul(v.c_str() + 1, &end, 16);
	if (*end) return false;
	out = 0xff000000 | uint32_t(n);
	return true;
}

static bool whole(const std::string &v, int lo, int hi, int &out)
{
	if (v.empty()) return false;
	char *end = nullptr;
	long n = strtol(v.c_str(), &end, 10);
	if (*end || n < lo || n > hi) return false;
	out = int(n);
	return true;
}

theme_spec parse_theme(const std::string &id, const std::string &text, std::vector<std::string> *problems)
{
	theme_spec sp;
	sp.id = sp.name = id;
	theme &c = sp.colours;
	const struct { const char *key; uint32_t *field; } cols[] = {
		{ "title_top", &c.title_top }, { "title_bottom", &c.title_bottom }, { "outline", &c.outline }, { "bg_top", &c.bg_top }, { "bg_bottom", &c.bg_bottom },
		{ "tab", &c.tab }, { "tab_sel", &c.tab_sel }, { "panel", &c.panel }, { "panel_edge", &c.panel_edge }, { "strip", &c.strip }, { "strip_text", &c.strip_text },
		{ "text", &c.text }, { "text_focus", &c.text_focus }, { "text_off", &c.text_off }, { "header", &c.header }, { "bar", &c.bar }, { "value", &c.value },
		{ "status", &c.status }, { "dialog", &c.dialog } };
	std::istringstream in(text);
	std::string line;
	int no = 0;
	while (std::getline(in, line))
	{
		no++;
		line = trim(line.substr(0, line.find(';')));
		if (line.empty()) continue;
		size_t eq = line.find('=');
		std::string key = eq == std::string::npos ? line : trim(line.substr(0, eq)), val = eq == std::string::npos ? "" : trim(line.substr(eq + 1));
		bool ok = eq != std::string::npos, known = false;
		for (auto &k : cols) if (key == k.key) { known = true; ok = ok && colour(val, *k.field); }
		if (known) {}
		else if (key == "name") { ok = ok && !val.empty(); if (ok) sp.name = val; }
		else if (key == "games") { sp.games.clear(); std::istringstream g(val); std::string w; while (g >> w) sp.games.push_back(w); }
		else if (key == "panel_alpha") ok = ok && whole(val, 0, 255, c.panel_alpha);
		else if (key == "background_dim") ok = ok && whole(val, 0, 255, sp.dim);
		else if (key == "title_font") ok = ok && whole(val, 0, 200, sp.title_font);
		else if (key == "list_font") ok = ok && whole(val, 0, 200, sp.list_font);
		else if (key == "background")
		{
			int w = 0, h = 0;
			size_t x = val.find('x');
			ok = ok && x != std::string::npos && whole(val.substr(0, x), 1, 4096, w) && whole(val.substr(x + 1), 1, 4096, h);
			if (ok) { sp.bg_w = w; sp.bg_h = h; }
		}
		else if (key == "background_crop")
		{
			int v[4] = { 0, 0, 0, 0 }, n = 0;
			std::istringstream g(val);
			std::string w;
			while (g >> w) { if (n >= 4 || !whole(w, n < 2 ? 0 : 1, 4096, v[n])) { n = -1; break; } n++; }
			ok = ok && n == 4;
			if (ok) std::copy(v, v + 4, sp.crop);
		}
		else ok = false;
		if (!ok && problems) problems->push_back(id + ".theme line " + std::to_string(no) + ": " + line);
	}
	return sp;
}

theme_spec load_theme_spec(const std::string &id, const std::string &data_dir)
{
	std::string text;
	if (!data_dir.empty())
		if (FILE *f = fopen((data_dir + "/themes/" + id + ".theme").c_str(), "rb"))
		{
			char buf[4096];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < 65536) text.append(buf, n);
			fclose(f);
		}
	if (text.empty()) if (const char *b = builtin_theme(id)) text = b;
	return parse_theme(id, text);
}

// ---------------------------------------------------------------- from a file to a theme
static uint32_t mixc(uint32_t a, uint32_t b, int t)   // t: 0 = a, 255 = b
{
	auto ch = [&](int sh) { return uint32_t((int((a >> sh) & 0xff) * (255 - t) + int((b >> sh) & 0xff) * t) / 255) << sh; };
	return 0xff000000 | ch(16) | ch(8) | ch(0);
}

// what the screens paint behind everything: the theme's ROM background, or its gradient
static std::vector<uint32_t> backdrop_of(const theme &th)
{
	const int W = screen::kW, H = screen::kH;
	if (th.backdrop.size() == size_t(W) * H) return th.backdrop;
	std::vector<uint32_t> out(size_t(W) * H);
	for (int y = 0; y < H; y++)
	{
		uint32_t c = mixc(th.bg_top, th.bg_bottom, y * 255 / (H - 1));
		std::fill(out.begin() + y * W, out.begin() + (y + 1) * W, c);
	}
	return out;
}

theme make_theme(const theme_spec &spec, const rom_art::art_set *art)
{
	if (!art || art->empty()) return parse_theme("neutral", builtin_theme("neutral")).colours;
	theme th = spec.colours;
	const int W = screen::kW, H = screen::kH;

	// the background: the first ROM background of the size the file names
	const rom_art::picture *bg = nullptr;
	if (spec.bg_w > 0)
		for (const rom_art::picture &p : art->backgrounds) if (p.w == spec.bg_w && p.h == spec.bg_h) { bg = &p; break; }
	if (bg)
	{
		int cx = spec.crop[0], cy = spec.crop[1], cw = spec.crop[2], ch = spec.crop[3];
		bool tile = cw > 0 && ch > 0 && cx + cw <= bg->w && cy + ch <= bg->h;
		th.backdrop = backdrop_of(th);                        // the gradient shows where the picture does not reach
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
			{
				int sx, sy;
				if (tile) { sx = cx + x % cw; sy = cy + y % ch; }
				else { sx = x - (W - bg->w) / 2; sy = y - (H - bg->h) / 2; }
				if (sx < 0 || sy < 0 || sx >= bg->w || sy >= bg->h) continue;
				uint32_t c = bg->at(sx, sy);
				if (c >> 24) th.backdrop[size_t(y) * W + x] = mixc(c, 0xff000000, std::clamp(spec.dim, 0, 255));
			}
	}

	// the lettering: the first ROM font of the named height, if it fits where it is used
	auto font_of = [&](int cap_h, int tallest) -> std::shared_ptr<const rom_art::font>
	{
		if (cap_h <= 0 || cap_h > tallest) return nullptr;
		for (const rom_art::font &f : art->fonts) if (f.cap_h == cap_h) return std::make_shared<rom_art::font>(f);
		return nullptr;
	};
	th.title_font = font_of(spec.title_font, 18);            // the title band is 19 rows
	th.list_font = font_of(spec.list_font, 9);               // a row is 12
	return th;
}

theme mix_themes(const theme &a, const theme &b, float t)
{
	if (t <= 0) return a;
	if (t >= 1) return b;
	const int k = std::clamp(int(t * 255 + 0.5f), 0, 255);
	theme out = k < 128 ? a : b;                              // the lettering changes over at the middle
	uint32_t theme::*cols[] = { &theme::title_top, &theme::title_bottom, &theme::outline, &theme::bg_top, &theme::bg_bottom, &theme::tab, &theme::tab_sel,
		&theme::panel, &theme::panel_edge, &theme::strip, &theme::strip_text, &theme::text, &theme::text_focus, &theme::text_off, &theme::header,
		&theme::bar, &theme::value, &theme::status, &theme::dialog };
	for (auto c : cols) out.*c = mixc(a.*c, b.*c, k);
	out.panel_alpha = (a.panel_alpha * (255 - k) + b.panel_alpha * k) / 255;
	if (a.backdrop.empty() && b.backdrop.empty()) out.backdrop.clear();
	else
	{
		std::vector<uint32_t> pa = backdrop_of(a), pb = backdrop_of(b);
		out.backdrop.resize(pa.size());
		for (size_t i = 0; i < pa.size(); i++) out.backdrop[i] = mixc(pa[i], pb[i], k);
	}
	return out;
}

} // namespace ui
