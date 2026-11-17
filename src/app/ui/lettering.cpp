// lettering.cpp - see lettering.h. The drawings below are this project's own: every glyph
// was drawn here on a pixel grid, with strokes two pixels thick. Rows are separated by
// '/', '#' is ink; an empty row is blank and missing rows at the bottom are blank.
#include "lettering.h"
#include <algorithm>
#include <array>

namespace ui
{

namespace
{

struct art { char ch; const char *rows; };

// ---------------------------------------------------------------- list size, 8 rows
const art kList[] = {
	{ ' ', "..../" },
	{ 'A', "..###../.#####./##...##/##...##/#######/#######/##...##/##...##" },
	{ 'B', "######./#######/##...##/######./######./##...##/#######/######." },
	{ 'C', ".#####./#######/##...##/##...../##...../##...##/#######/.#####." },
	{ 'D', "######./#######/##...##/##...##/##...##/##...##/#######/######." },
	{ 'E', "#######/#######/##...../#####../#####../##...../#######/#######" },
	{ 'F', "#######/#######/##...../#####../#####../##...../##...../##....." },
	{ 'G', ".#####./#######/##...../##.####/##.####/##...##/#######/.#####." },
	{ 'H', "##...##/##...##/##...##/#######/#######/##...##/##...##/##...##" },
	{ 'I', "####/####/.##./.##./.##./.##./####/####" },
	{ 'J', ".....##/.....##/.....##/.....##/##...##/##...##/#######/.#####." },
	{ 'K', "##...##/##..###/##.###./#####../#####../##.###./##..###/##...##" },
	{ 'L', "##...../##...../##...../##...../##...../##...../#######/#######" },
	{ 'M', "##.....##/###...###/####.####/#########/##.###.##/##..#..##/##.....##/##.....##" },
	{ 'N', "##....##/###...##/####..##/#####.##/##.#####/##..####/##...###/##....##" },
	{ 'O', ".#####./#######/##...##/##...##/##...##/##...##/#######/.#####." },
	{ 'P', "######./#######/##...##/#######/######./##...../##...../##....." },
	{ 'Q', ".#####../#######./##...##./##...##./##.#.##./##..###./#######./.####.##" },
	{ 'R', "######./#######/##...##/#######/######./##.###./##..###/##...##" },
	{ 'S', ".######/#######/##...../######./.######/.....##/#######/######." },
	{ 'T', "########/########/...##.../...##.../...##.../...##.../...##.../...##..." },
	{ 'U', "##...##/##...##/##...##/##...##/##...##/##...##/#######/.#####." },
	{ 'V', "##...##/##...##/##...##/##...##/##...##/.##.##./.#####./..###.." },
	{ 'W', "##.....##/##.....##/##..#..##/##.###.##/#########/####.####/###...###/##.....##" },
	{ 'X', "##...##/##...##/.##.##./..###../..###../.##.##./##...##/##...##" },
	{ 'Y', "##....##/##....##/.##..##./..####../...##.../...##.../...##.../...##..." },
	{ 'Z', "#######/#######/....###/...###./..###../.###.../#######/#######" },
	{ '0', ".#####./#######/##..###/##.#.##/###..##/##...##/#######/.#####." },
	{ '1', "..##../.###../####../..##../..##../..##../######/######" },
	{ '2', ".#####./#######/##...##/....###/..####./.###.../#######/#######" },
	{ '3', "######./#######/.....##/..####./..####./.....##/#######/######." },
	{ '4', "...###./..####./.##.##./##..##./#######/#######/....##./....##." },
	{ '5', "#######/#######/##...../######./#######/.....##/#######/######." },
	{ '6', ".#####./#######/##...../######./#######/##...##/#######/.#####." },
	{ '7', "#######/#######/.....##/....##./...##../..##.../..##.../..##..." },
	{ '8', ".#####./#######/##...##/.#####./.#####./##...##/#######/.#####." },
	{ '9', ".#####./#######/##...##/#######/.######/.....##/#######/.#####." },
	{ '!', "##/##/##/##/##//##/##" },
	{ '"', "##.##/##.##/##.##" },
	{ '#', ".##..##./.##..##./########/.##..##./.##..##./########/.##..##./.##..##." },
	{ '$', "...#.../.######/##.#.../######./.######/...#.##/######./...#..." },
	{ '%', ".#...##/#.#.##./.#.##../..##.../.##.#../##.#.#./#...#.." },
	{ '&', ".####.../##..##../##..##../.####.../####.###/##.####./##..###./.####.##" },
	{ '\'', "##/##/##" },
	{ '(', "..##/.##./##../##../##../##../.##./..##" },
	{ ')', "##../.##./..##/..##/..##/..##/.##./##.." },
	{ '*', "/##..##/.####./######/.####./##..##" },
	{ '+', "/..##../..##../######/######/..##../..##.." },
	{ ',', ".../////.##/.##/##." },
	{ '-', ".....///#####/#####" },
	{ '.', "..//////##/##" },
	{ '/', "....##/....##/...##./..##../..##../.##.../##..../##...." },
	{ ':', "../##/##///##/##" },
	{ ';', ".../.##/.##///.##/.##/##." },
	{ '<', "...##/..##./.##../##.../##.../.##../..##./...##" },
	{ '=', "....../######/######///######/######" },
	{ '>', "##.../.##../..##./...##/...##/..##./.##../##..." },
	{ '?', ".#####./#######/##...##/....###/...##..//...##../...##.." },
	{ '@', ".######./########/##....##/##.##.##/##.#####/##....../########/.######." },
	{ '[', "####/####/##../##../##../##../####/####" },
	{ '\\', "##..../##..../.##.../..##../..##../...##./....##/....##" },
	{ ']', "####/####/..##/..##/..##/..##/####/####" },
	{ '^', "..##../.####./##..##" },
	{ '_', ".......//////#######/#######" },
	{ '`', "##./.##" },
	{ '{', "..###/.##../.##../##.../##.../.##../.##../..###" },
	{ '|', "##/##/##/##/##/##/##/##" },
	{ '}', "###../..##./..##./...##/...##/..##./..##./###.." },
	{ '~', ".......///.###.##/##.###." },
	{ '\x01', ".......//...#.../..###../.#####./#######" },
	{ '\x02', ".......//#######/.#####./..###../...#..." },
};

// ---------------------------------------------------------------- note size, 7 rows
const art kNote[] = {
	{ ' ', ".../" },
	{ 'A', ".####./##..##/##..##/######/##..##/##..##/##..##" },
	{ 'B', "#####./##..##/##..##/#####./##..##/##..##/#####." },
	{ 'C', ".####./##..##/##..../##..../##..../##..##/.####." },
	{ 'D', "#####./##..##/##..##/##..##/##..##/##..##/#####." },
	{ 'E', "######/##..../##..../#####./##..../##..../######" },
	{ 'F', "######/##..../##..../#####./##..../##..../##...." },
	{ 'G', ".####./##..##/##..../##.###/##..##/##..##/.####." },
	{ 'H', "##..##/##..##/##..##/######/##..##/##..##/##..##" },
	{ 'I', "####/.##./.##./.##./.##./.##./####" },
	{ 'J', "....##/....##/....##/....##/##..##/##..##/.####." },
	{ 'K', "##..##/##.##./####../###.../####../##.##./##..##" },
	{ 'L', "##..../##..../##..../##..../##..../##..../######" },
	{ 'M', "##...##/###.###/#######/##.#.##/##...##/##...##/##...##" },
	{ 'N', "##...##/###..##/####.##/##.####/##..###/##...##/##...##" },
	{ 'O', ".####./##..##/##..##/##..##/##..##/##..##/.####." },
	{ 'P', "#####./##..##/##..##/#####./##..../##..../##...." },
	{ 'Q', ".####./##..##/##..##/##..##/##.###/##.##./.###.#" },
	{ 'R', "#####./##..##/##..##/#####./####../##.##./##..##" },
	{ 'S', ".#####/##..../##..../.####./....##/....##/#####." },
	{ 'T', "######/..##../..##../..##../..##../..##../..##.." },
	{ 'U', "##..##/##..##/##..##/##..##/##..##/##..##/.####." },
	{ 'V', "##..##/##..##/##..##/##..##/##..##/.####./..##.." },
	{ 'W', "##...##/##...##/##...##/##.#.##/#######/###.###/##...##" },
	{ 'X', "##..##/##..##/.####./..##../.####./##..##/##..##" },
	{ 'Y', "##..##/##..##/.####./..##../..##../..##../..##.." },
	{ 'Z', "######/....##/...##./..##../.##.../##..../######" },
	{ '0', ".####./##..##/##.###/###.##/##..##/##..##/.####." },
	{ '1', ".##./###./.##./.##./.##./.##./####" },
	{ '2', ".####./##..##/....##/...##./..##../.##.../######" },
	{ '3', "#####./....##/....##/.####./....##/....##/#####." },
	{ '4', "...###/..####/.##.##/##..##/######/....##/....##" },
	{ '5', "######/##..../#####./....##/....##/##..##/.####." },
	{ '6', ".####./##..../##..../#####./##..##/##..##/.####." },
	{ '7', "######/....##/...##./..##../..##../..##../..##.." },
	{ '8', ".####./##..##/##..##/.####./##..##/##..##/.####." },
	{ '9', ".####./##..##/##..##/.#####/....##/....##/.####." },
	{ '!', "##/##/##/##//##/##" },
	{ '"', "##.##/##.##" },
	{ '#', ".##.##./.##.##./#######/.##.##./#######/.##.##./.##.##." },
	{ '$', "..##../.#####/##..../.####./....##/#####./..##.." },
	{ '%', "##..##/##..##/...##./..##../.##.../##..##/##..##" },
	{ '&', ".###.../##.##../##.##../.###.../##.####/##..##./.###.##" },
	{ '\'', "##/##" },
	{ '(', ".##/##./##./##./##./##./.##" },
	{ ')', "##./.##/.##/.##/.##/.##/##." },
	{ '*', "/##..##/.####./######/.####./##..##" },
	{ '+', "/..##../..##../######/..##../..##.." },
	{ ',', "...////.##/.##/##." },
	{ '-', ".....///#####" },
	{ '.', "../////##/##" },
	{ '/', "....##/....##/...##./..##../.##.../##..../##...." },
	{ ':', "../##/##//##/##" },
	{ ';', ".../.##/.##//.##/.##/##." },
	{ '<', "...##/..##./.##../##.../.##../..##./...##" },
	{ '=', ".....//#####//#####" },
	{ '>', "##.../.##../..##./...##/..##./.##../##..." },
	{ '?', ".####./##..##/....##/...##./..##..//..##.." },
	{ '@', ".#####./##...##/##.####/##.#.##/##.####/##...../.#####." },
	{ '[', "###/##./##./##./##./##./###" },
	{ '\\', "##..../##..../.##.../..##../...##./....##/....##" },
	{ ']', "###/.##/.##/.##/.##/.##/###" },
	{ '^', "..##../.####./##..##" },
	{ '_', "......//////######" },
	{ '`', "##./.##" },
	{ '{', "..##/.##./.##./##../.##./.##./..##" },
	{ '|', "##/##/##/##/##/##/##" },
	{ '}', "##../.##./.##./..##/.##./.##./##.." },
	{ '~', ".......//.###..#/#..###." },
	{ '\x01', ".......//...#.../..###../.#####." },
	{ '\x02', ".......//.#####./..###../...#..." },
};

struct glyph { int w = 0; std::vector<uint8_t> m; bool own = false; };

struct faces
{
	std::array<glyph, 128> g[3];
	std::vector<std::string> errors;
	int h[3] = { 7, 8, 16 };

	void load(const art *a, size_t n, int f)
	{
		for (size_t i = 0; i < n; i++)
		{
			std::vector<std::string> rows(1);
			for (const char *p = a[i].rows; *p; p++) { if (*p == '/') rows.emplace_back(); else rows.back() += *p; }
			size_t w = 0;
			for (auto &r : rows) w = std::max(w, r.size());
			std::string name = std::string(f == 0 ? "note '" : "list '") + a[i].ch + "'";
			if (int(rows.size()) > h[f]) errors.push_back(name + ": too many rows");
			for (auto &r : rows) if (!r.empty() && r.size() != w) errors.push_back(name + ": rows of unequal length");
			glyph &gl = g[f][size_t(a[i].ch) & 127];
			if (gl.own) errors.push_back(name + ": drawn twice");
			gl.w = int(w); gl.own = true;
			gl.m.assign(w * size_t(h[f]), 0);
			for (size_t y = 0; y < rows.size() && int(y) < h[f]; y++)
				for (size_t x = 0; x < rows[y].size(); x++)
				{
					if (rows[y][x] != '#' && rows[y][x] != '.') errors.push_back(name + ": stray character");
					gl.m[y * w + x] = rows[y][x] == '#';
				}
		}
	}

	// the list drawing at twice the size, corners smoothed (the EPX rule)
	static glyph doubled(const glyph &s, int sh)
	{
		glyph d;
		d.w = s.w * 2; d.own = s.own;
		d.m.assign(size_t(d.w) * size_t(sh) * 2, 0);
		auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < s.w && y < sh ? s.m[size_t(y) * s.w + x] : uint8_t(0); };
		for (int y = 0; y < sh; y++)
			for (int x = 0; x < s.w; x++)
			{
				uint8_t p = at(x, y), a = at(x, y - 1), b = at(x + 1, y), c = at(x - 1, y), e = at(x, y + 1);
				uint8_t o[4] = { p, p, p, p };
				if (c == a && c != e && a != b) o[0] = a;
				if (a == b && a != c && b != e) o[1] = b;
				if (e == c && e != b && c != a) o[2] = c;
				if (b == e && b != a && e != c) o[3] = e;
				for (int k = 0; k < 4; k++) d.m[size_t(y * 2 + k / 2) * d.w + size_t(x * 2 + k % 2)] = o[k];
			}
		return d;
	}

	faces()
	{
		load(kNote, sizeof(kNote) / sizeof(kNote[0]), 0);
		load(kList, sizeof(kList) / sizeof(kList[0]), 1);
		for (int c = 0; c < 128; c++) g[2][size_t(c)] = doubled(g[1][size_t(c)], h[1]);
	}

	const glyph &get(char ch, int f) const
	{
		unsigned c = static_cast<unsigned char>(ch);
		if (c >= 'a' && c <= 'z') c -= 32;
		if (c > 127 || !g[f][c].own) c = '?';
		return g[f][c];
	}
};

const faces &all() { static const faces f; return f; }
int gap(face f) { return f == face::title ? 2 : 1; }

uint32_t mixc(uint32_t a, uint32_t b, int t)   // t: 0 = a, 255 = b
{
	auto ch = [&](int sh) { return uint32_t((int((a >> sh) & 0xff) * (255 - t) + int((b >> sh) & 0xff) * t) / 255) << sh; };
	return 0xff000000 | ch(16) | ch(8) | ch(0);
}

void dot(canvas &cv, int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < cv.w && y < cv.h) cv.px[size_t(y) * cv.w + x] = c; }

} // namespace

ink ink_of(uint32_t colour, uint32_t outline) { return { mixc(colour, 0xffffffff, 80), mixc(colour, 0xff000000, 70), outline }; }

bool has_glyph(char ch, face f)
{
	unsigned c = static_cast<unsigned char>(ch);
	return c < 128 && all().g[int(f)][c].own;
}

int text_h(face f) { return all().h[int(f)]; }

int text_w(const std::string &s, face f)
{
	int w = 0;
	for (char ch : s) w += all().get(ch, int(f)).w + gap(f);
	return s.empty() ? 0 : w - gap(f);
}

// One letter as a mask of w x h cells; pass 0 paints its outline, pass 1 the letter itself:
// a top-to-bottom gradient with a bevel (edges that face up or left catch the light, the
// others are in shade).
template <class At>
static void paint(canvas &cv, int x, int y, int w, int h, At at, const ink &k, int pass, int reach, int lit_by, int shade_by)
{
	for (int py = 0; py < h; py++)
		for (int px = 0; px < w; px++)
		{
			if (!at(px, py)) continue;
			if (pass == 0)
			{
				for (int dy = -1; dy <= reach; dy++) for (int dx = -1; dx <= reach; dx++) dot(cv, x + px + dx, y + py + dy, k.outline);
				continue;
			}
			uint32_t c = mixc(k.top, k.bottom, py * 255 / std::max(1, h - 1));
			bool lit = !at(px, py - 1) || !at(px - 1, py), shade = !at(px, py + 1) || !at(px + 1, py);
			if (lit && !shade) c = mixc(c, 0xffffffff, lit_by);
			else if (shade && !lit) c = mixc(c, 0xff000000, shade_by);
			dot(cv, x + px, y + py, c);
		}
}

void draw_text(canvas &cv, int x, int y, const std::string &s, face f, const ink &k)
{
	const faces &fs = all();
	const int h = fs.h[int(f)], reach = f == face::title ? 2 : 1;
	// first the outline of the whole text, then the letters, so no outline covers a letter
	for (int pass = 0; pass < 2; pass++)
	{
		int gx = x;
		for (char ch : s)
		{
			const glyph &g = fs.get(ch, int(f));
			auto at = [&](int px, int py) { return px >= 0 && py >= 0 && px < g.w && py < h && g.m[size_t(py) * g.w + px]; };
			paint(cv, gx, y, g.w, h, at, k, pass, reach, f == face::note ? 50 : 95, f == face::note ? 40 : 75);
			gx += g.w + gap(f);
		}
	}
}

// ---------------------------------------------------------------- the old screens' text
// A word that only reads right with its lower case (a file name, a folder, a web address)
// stays in the plain 8x8 font; the lettering has capitals only.
static bool needs_case(const std::string &w)
{
	bool lower = false, mark = false;
	for (size_t i = 0; i < w.size(); i++)
	{
		char c = w[i];
		lower = lower || (c >= 'a' && c <= 'z');
		mark = mark || c == '/' || c == char(92) || c == '_' || (c == '.' && i + 1 < w.size() && w[i + 1] >= 'a' && w[i + 1] <= 'z');
	}
	return lower && mark && w.size() > 3;
}

void cell_text(canvas &cv, int x, int y, const std::string &s, uint32_t c, int scale)
{
	scale = std::max(1, scale);
	// one cell high: the note size; two and more: the title size, doubled again for four
	const face f = scale >= 2 ? face::title : face::note;
	const int mul = scale >= 2 ? scale / 2 : 1, cell = 8 * scale, h = all().h[int(f)] * mul;
	const ink k = ink_of(c);
	size_t i = 0;
	while (i < s.size())
	{
		size_t e = s.find(' ', i);
		if (e == std::string::npos) e = s.size();
		const std::string word = s.substr(i, e - i);
		const int wx = x + int(i) * cell;
		if (needs_case(word)) cv.text_plain(wx, y, word, c, scale, true);
		else
			for (int pass = 0; pass < 2; pass++)
				for (size_t n = 0; n < word.size(); n++)
				{
					const glyph &g = all().get(word[n], int(f));
					auto at = [&](int px, int py) { return px >= 0 && py >= 0 && px < g.w * mul && py < h && g.m[size_t(py / mul) * g.w + size_t(px / mul)]; };
					// in the middle of its cell; a letter wider than the cell leans on its neighbours
					paint(cv, wx + int(n) * cell + (cell - g.w * mul) / 2, y, g.w * mul, h, at, k, pass, 1, f == face::note ? 50 : 95, f == face::note ? 40 : 75);
				}
		i = e + 1;
	}
}

// ---------------------------------------------------------------- a game's own lettering
namespace
{
// the project's list drawing of a character, stretched to `h` rows (nearest cell)
struct stretched
{
	const glyph &g;
	int h, w;
	stretched(char ch, int height) : g(all().get(ch, int(face::list))), h(std::max(1, height)), w(std::max(1, (g.w * std::max(1, height) + 4) / 8)) {}
	bool at(int px, int py) const
	{
		if (px < 0 || py < 0 || px >= w || py >= h) return false;
		return g.m[size_t(py * 8 / h) * g.w + size_t(px * g.w / w)] != 0;
	}
};
int space_w(const rom_art::font &f) { return std::max(2, f.cap_h / 3); }
int lum(uint32_t c) { return int((c >> 16) & 0xff) * 3 + int((c >> 8) & 0xff) * 6 + int(c & 0xff); }
}

ink ink_from(const rom_art::font &f)
{
	// from its capital A: the darkest colour is the outline; the letter's colour near the
	// top and near the bottom, leaving the outline out, make the gradient
	const rom_art::picture &a = f.glyph['A'];
	ink k = { 0xffffffff, 0xffc0c0c0, 0xff000000 };
	if (a.empty()) return k;
	int darkest = 1 << 30, brightest = -1;
	for (uint32_t c : a.px) if (c >> 24) { if (lum(c) < darkest) { darkest = lum(c); k.outline = c; } brightest = std::max(brightest, lum(c)); }
	auto band = [&](int y0, int y1, uint32_t &out)
	{
		long r = 0, g = 0, b = 0, n = 0;
		for (int y = std::max(0, y0); y < std::min(a.h, y1); y++)
			for (int x = 0; x < a.w; x++)
			{
				uint32_t c = a.at(x, y);
				if (!(c >> 24) || lum(c) * 2 < darkest + brightest) continue;
				r += (c >> 16) & 0xff; g += (c >> 8) & 0xff; b += c & 0xff; n++;
			}
		if (n) out = 0xff000000 | uint32_t(r / n) << 16 | uint32_t(g / n) << 8 | uint32_t(b / n);
	};
	band(0, std::max(1, a.h * 2 / 5), k.top);
	k.bottom = k.top;
	band(a.h - std::max(1, a.h * 2 / 5), a.h, k.bottom);
	return k;
}

int text_w(const std::string &s, const rom_art::font &f)
{
	int w = 0;
	for (char ch : s)
		w += (ch == ' ' ? space_w(f) : f.has(ch) ? f.glyph[static_cast<unsigned char>(ch)].w : stretched(ch, f.cap_h).w) + 1;
	return s.empty() ? 0 : w - 1;
}

void draw_text(canvas &cv, int x, int y, const std::string &s, const rom_art::font &f, uint32_t tint)
{
	auto tinted = [tint](uint32_t c)
	{
		auto ch = [&](int sh) { return uint32_t(((c >> sh) & 0xff) * ((tint >> sh) & 0xff) / 255) << sh; };
		return 0xff000000 | ch(16) | ch(8) | ch(0);
	};
	ink k = ink_from(f);
	k.top = tinted(k.top); k.bottom = tinted(k.bottom); k.outline = tinted(k.outline);
	for (char ch : s)
	{
		if (ch == ' ') { x += space_w(f) + 1; continue; }
		if (f.has(ch))
		{
			// letters stand on one line; a taller one (a Q's tail) hangs below it
			const rom_art::picture &g = f.glyph[static_cast<unsigned char>(ch)];
			int gy = y + std::max(0, f.cap_h - g.h);
			// the games place these by a per-letter offset that is not read: quote marks
			// belong at the top of the line and dashes in its middle
			if (g.h < f.cap_h && (ch == 39 || ch == 34 || ch == 96 || ch == 94 || ch == 42)) gy = y;
			else if (g.h < f.cap_h && (ch == 45 || ch == 43 || ch == 61 || ch == 126)) gy = y + (f.cap_h - g.h) / 2;
			for (int py = 0; py < g.h; py++) for (int px = 0; px < g.w; px++) if (g.at(px, py) >> 24) dot(cv, x + px, gy + py, tinted(g.at(px, py)));
			x += g.w + 1;
			continue;
		}
		stretched st(ch, f.cap_h);
		auto at = [&](int px, int py) { return st.at(px, py); };
		for (int pass = 0; pass < 2; pass++) paint(cv, x, y, st.w, st.h, at, k, pass, 1, 60, 50);
		x += st.w + 1;
	}
}

std::string fit_px(const std::string &s, int px, const measure_fn &width)
{
	if (px <= 0) return "";
	if (width(s) <= px) return s;
	std::string t = s;
	while (!t.empty())
	{
		t.pop_back();
		while (!t.empty() && t.back() == ' ') t.pop_back();
		if (width(t + "..") <= px) return t + "..";
	}
	return "";
}

std::string marquee_px(const std::string &s, int px, const measure_fn &width, unsigned now_ms)
{
	if (px <= 0) return "";
	if (width(s) <= px) return s;
	// how far the window has to travel for the end of the text to show
	unsigned over = 0;
	while (over < s.size() && width(s.substr(over)) > px) over++;
	const unsigned pause = 900, per_char = 170;
	unsigned cycle = pause * 2 + over * per_char, t = now_ms % cycle;
	unsigned off = t < pause ? 0 : std::min(over, (t - pause) / per_char);
	std::string win = s.substr(off);
	while (!win.empty() && width(win) > px) win.pop_back();
	return win;
}

std::vector<std::string> wrap_px(const std::string &s, int px, const measure_fn &width, int max_lines)
{
	std::vector<std::string> out;
	std::string rest = s;
	while (!rest.empty() && int(out.size()) < max_lines)
	{
		if (int(out.size()) == max_lines - 1) { out.push_back(fit_px(rest, px, width)); break; }
		size_t take = rest.size();
		if (width(rest) > px)
		{
			// the last space that still leaves the line narrow enough
			take = 0;
			for (size_t i = 1; i < rest.size(); i++)
				if (rest[i] == ' ') { if (width(rest.substr(0, i)) <= px) take = i; else break; }
			if (take == 0) { take = 1; while (take < rest.size() && width(rest.substr(0, take + 1)) <= px) take++; }
		}
		out.push_back(rest.substr(0, take));
		rest = rest.substr(take);
		while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
	}
	return out;
}

std::string fit_px(const std::string &s, int px, face f) { return fit_px(s, px, [f](const std::string &t) { return text_w(t, f); }); }
std::string marquee_px(const std::string &s, int px, face f, unsigned now_ms) { return marquee_px(s, px, [f](const std::string &t) { return text_w(t, f); }, now_ms); }
std::vector<std::string> wrap_px(const std::string &s, int px, face f, int max_lines) { return wrap_px(s, px, [f](const std::string &t) { return text_w(t, f); }, max_lines); }

std::vector<std::string> lettering_errors() { return all().errors; }

void draw_sheet(canvas &cv, const ink &title, const ink &text)
{
	static const char *lines[] = { "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "0123456789 .,:;!?'\"", "#$%&()*+-/<=>", "@[\\]^_`{|}~ \x01\x02" };
	int y = 6;
	for (face f : { face::title, face::list, face::note })
	{
		for (const char *l : lines) { draw_text(cv, 8, y, l, f, f == face::title ? title : text); y += text_h(f) + (f == face::title ? 5 : 4); }
		y += 4;
	}
	draw_text(cv, 8, y, "He's on fire! 12 teams, 4 players.", face::list, text);
	draw_text(cv, 8, y + 13, "Lower case is shown as capitals.", face::note, text);
}

} // namespace ui
