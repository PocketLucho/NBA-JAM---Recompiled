// arttest - the ROM art reader (roadmap U2.2, src/app/rom_art.cpp).
//
//   arttest [out_dir]
//       No ROM needed (this is what CI runs): builds a small made-up program ROM and
//       graphics ROM in memory, holding a font table, a logo table, a background module
//       and a compressed picture, and checks that each is read and decoded pixel for pixel; then that wrong, cut and missing ROMs
//       give the fallback and never a crash. With a folder, PNGs of the test screen.
//
//   arttest --roms <app_dir> [out_dir]
//       The user's own ROMs (<app_dir>/roms): what is read from each game, and with a
//       folder the pages of the test screen as PNGs. Those PNGs hold game art: keep the
//       folder outside the repository.
#include "games.h"
#include "rom_art.h"
#include "ui/art_view.h"
#include "ui/gallery.h"
#include "ui/themes.h"
#include "miniz.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace rom_art;

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (line %d)\n", std::string(msg).c_str(), __LINE__); } else printf("ok    %s\n", std::string(msg).c_str()); } while (0)

static void write_png(const std::string &dir, const std::string &name, const canvas &cv)
{
	if (dir.empty()) return;
	std::vector<uint8_t> rgb(size_t(cv.w) * cv.h * 3);
	for (size_t i = 0; i < cv.px.size(); i++) { rgb[i * 3] = uint8_t(cv.px[i] >> 16); rgb[i * 3 + 1] = uint8_t(cv.px[i] >> 8); rgb[i * 3 + 2] = uint8_t(cv.px[i]); }
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), cv.w, cv.h, 3, &len);
	FILE *f = fopen((dir + "/" + name).c_str(), "wb");
	if (f && png) fwrite(png, 1, len, f);
	if (f) fclose(f);
	mz_free(png);
}

// ---------------------------------------------------------------- a made-up ROM
struct maker
{
	roms r;
	uint64_t gbit = 0x2000 * 8;              // next free bit of the graphics ROM
	maker() { r.program.assign(0x20000, 0); r.gfx.assign(0x8000, 0); }
	static uint32_t addr(size_t off) { return kProgramBase + uint32_t(off) * 8; }
	void w16(size_t o, unsigned v) { r.program[o] = uint8_t(v); r.program[o + 1] = uint8_t(v >> 8); }
	void l32(size_t o, uint32_t v) { w16(o, v & 0xffff); w16(o + 2, v >> 16); }
	void bits(unsigned v, unsigned n) { for (unsigned i = 0; i < n; i++, gbit++) if (v >> i & 1) r.gfx[size_t(gbit >> 3)] |= uint8_t(1u << (gbit & 7)); }
	// a palette of n colours: colour i is 15-bit value `first + i`
	void palette(size_t o, unsigned n, unsigned first) { w16(o, n); for (unsigned i = 0; i < n; i++) w16(o + 2 + i * 2, first + i); }
	uint32_t sag() const { return 0x2000000u + uint32_t(gbit); }
	void header(size_t o, int w, int h, uint32_t sag_, unsigned ctrl, size_t pal) { w16(o, unsigned(w)); w16(o + 2, unsigned(h)); l32(o + 8, sag_); w16(o + 12, ctrl); l32(o + 14, addr(pal)); }
};

// the colour the reader gives a 15-bit palette value
static uint32_t rgb_of(unsigned c)
{
	auto ch = [&](int sh) { return uint32_t(((c >> sh) & 31) * 255 / 31); };
	return 0xff000000 | ch(10) << 16 | ch(5) << 8 | ch(0);
}
// pixel value of the made-up glyph for character c
static unsigned glyph_px(int c, int x, int y) { return (x == 0 && y == 0) ? 0u : unsigned((x + y + c) % 3) + 1; }
static int glyph_w(int c) { return 3 + c % 4; }

static roms synthetic()
{
	maker m;
	const size_t kPal = 0x1000, kGlyphs = 0x2000, kPacked = 0x3000, kLogoPals = 0x4000, kLogoHdrs = 0x6000;
	const size_t kFont = 0x8000, kFaces = 0x8800, kLogos = 0x9000, kMod = 0xb000, kBlocks = 0xb100, kBgHdrs = 0xb200, kBgPals = 0xb300;
	m.palette(kPal, 4, 0x1000);

	// lettering: capitals, digits and '!', 2 bits a pixel, rows packed with no padding
	int i = 0;
	for (int c = 0x21; c < 0x5b; c++)
	{
		if (!(c == '!' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) continue;
		size_t h = kGlyphs + size_t(i++) * 18;
		m.header(h, glyph_w(c), 5, m.sag(), 0x2000, kPal);
		for (int y = 0; y < 5; y++) for (int x = 0; x < glyph_w(c); x++) m.bits(glyph_px(c, x, y), 2);
		m.l32(kFont + size_t(c) * 4, maker::addr(h));
		if (c >= 'A') m.l32(kFont + size_t(c + 32) * 4, maker::addr(h));
	}

	// one compressed picture, 12 x 3 at 8 bits: each row leaves out 2 pixels at its start
	// (1 << shift 1) and 1 at its end
	m.header(kPacked, 12, 3, m.sag(), 0x0180, kPal);
	for (int y = 0; y < 3; y++) { m.bits(0x11, 8); for (int x = 0; x < 9; x++) m.bits(unsigned(x % 3) + 1, 8); }

	// 27 logos and 40 faces: compressed 8-bit pictures, a palette each
	uint32_t pix = m.sag();
	for (int y = 0; y < 10; y++) { m.bits(0x00, 8); for (int x = 0; x < 12; x++) m.bits(1, 8); }
	for (int k = 0; k < 67; k++)
	{
		m.palette(kLogoPals + size_t(k) * 8, 2, 0x2000 + unsigned(k) * 2);
		m.header(kLogoHdrs + size_t(k) * 18, 12, 10, pix, 0x0080, kLogoPals + size_t(k) * 8);
	}
	for (int k = 0; k < 40; k++) m.l32(kFaces + size_t(k) * 4, maker::addr(kLogoHdrs + size_t(27 + k) * 18));   // looks the same, is not the logos
	for (int k = 0; k < 27; k++) m.l32(kLogos + size_t(k) * 4, maker::addr(kLogoHdrs + size_t(k) * 18));

	// a background module of two blocks of one 8 x 4 picture, the second mirrored, on a higher layer
	uint32_t bgpix = m.sag();
	for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) m.bits(x == 0 ? 3 : 1 + unsigned(y & 1), 4);
	m.w16(kBgHdrs, 8); m.w16(kBgHdrs + 2, 4); m.l32(kBgHdrs + 4, bgpix); m.w16(kBgHdrs + 8, 0x4000);
	m.l32(kBgPals, maker::addr(kPal));
	m.w16(kBlocks, 0x0110); m.w16(kBlocks + 2, 100); m.w16(kBlocks + 4, 50);      // listed first, but on layer 1: drawn last
	m.w16(kBlocks + 8, 0x0000); m.w16(kBlocks + 10, 0); m.w16(kBlocks + 12, 0);
	m.w16(kMod, 400); m.w16(kMod + 2, 254); m.w16(kMod + 4, 2);
	m.l32(kMod + 6, maker::addr(kBlocks)); m.l32(kMod + 10, maker::addr(kBgHdrs)); m.l32(kMod + 14, maker::addr(kBgPals));
	return m.r;
}

// ---------------------------------------------------------------- the user's ROMs
static int real_roms(const std::string &base, const std::string &out)
{
	game_catalog cat(base);
	ui::theme th;
	int found = 0;
	for (const game_info &g : game_list())
	{
		roms r;
		bool have = cat.art_roms(g, r);
		art_set s = have ? read(r) : art_set();
		std::string fonts;
		for (auto &f : s.fonts) fonts += " " + std::to_string(f.cap_h) + "px/" + std::to_string(f.count());
		printf("%-9s %s  fonts %zu (%s )  logos %zu  backgrounds %zu\n", g.id, have ? "ROMs found  " : "no ROMs     ", s.fonts.size(), fonts.c_str(), s.logos.size(), s.backgrounds.size());
		found += !s.empty();
		for (int p = 0; p < ui::art_pages(s); p++)
		{
			canvas cv(ui::screen::kW, ui::screen::kH);
			ui::draw_art_page(cv, th, g.title, s, p, ui::device::xbox);
			write_png(out, std::string(g.id) + "_page" + std::to_string(p + 1) + ".png", cv);
		}
	}
	// the gallery in each family's theme, with that game's art
	for (const game_info &g : game_list())
	{
		roms r;
		art_set s = cat.art_roms(g, r) ? read(r) : art_set();
		ui::theme_spec sp = ui::load_theme_spec(ui::theme_id_for_game(g.id), "");
		ui::theme t = ui::make_theme(sp, &s);
		printf("%-9s theme %-9s background %s  title font %s  list font %s\n", g.id, sp.id.c_str(), t.backdrop.empty() ? "no " : "yes",
		       t.title_font ? std::to_string(t.title_font->cap_h).c_str() : "project", t.list_font ? std::to_string(t.list_font->cap_h).c_str() : "project");
		for (int tab = 0; tab < 2; tab++)
		{
			ui::screen scr = ui::make_gallery();
			scr.title = g.title;
			scr.cur_tab = tab * 2; scr.cur_panel = 0; scr.fix_focus();
			canvas cv(ui::screen::kW, ui::screen::kH);
			scr.draw(cv, t, ui::device::xbox, 0);
			write_png(out, std::string(g.id) + "_theme" + std::to_string(tab + 1) + ".png", cv);
		}
		ui::theme half = ui::mix_themes(ui::theme(), t, 0.5f);
		ui::screen scr = ui::make_gallery();
		canvas cv(ui::screen::kW, ui::screen::kH);
		scr.draw(cv, half, ui::device::xbox, 0);
		write_png(out, std::string(g.id) + "_theme_halfway.png", cv);
	}
	printf("\n%d of %zu games gave ROM art\n", found, game_list().size());
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "--roms")) return real_roms(argv[2], argc > 3 ? argv[3] : "");
	std::string out = argc > 1 ? argv[1] : "";
	ui::theme th;

	const roms rom = synthetic();
	art_set s = read(rom);
	std::vector<picture> s_logos;
	const uint32_t c1 = rgb_of(0x1001), c2 = rgb_of(0x1002), c3 = rgb_of(0x1003);

	// ---------------- lettering ----------------
	CHECK(s.fonts.size() == 1, "lettering: the font table is found by its shape (" + std::to_string(s.fonts.size()) + " found)");
	if (s.fonts.size() == 1)
	{
		const font &f = s.fonts[0];
		CHECK(f.cap_h == 5 && f.count() == 37 + 26 && f.palette.size() == 4, "lettering: 26 capitals, 10 digits and '!' are read, with the palette");
		bool exact = true;
		for (int c : { 'A', 'B', 'G', 'Z', '0', '7', '!' })
		{
			const picture &g = f.glyph[c];
			exact = exact && g.w == glyph_w(c) && g.h == 5;
			for (int y = 0; y < g.h && exact; y++)
				for (int x = 0; x < g.w && exact; x++)
				{
					unsigned v = glyph_px(c, x, y);
					exact = g.at(x, y) == (v ? rgb_of(0x1000 + v) : 0u);
				}
		}
		CHECK(exact, "lettering: letters of every width decode pixel for pixel (rows are packed without padding)");
		CHECK(f.glyph['A'].at(0, 0) == 0 && (f.glyph['A'].at(1, 0) >> 24) == 255, "lettering: palette entry 0 is see-through, the rest solid");
		CHECK(f.has('a') && f.glyph['a'].px == f.glyph['A'].px, "lettering: lower case uses the capitals, as the games' tables do");
		CHECK(!f.has('#') && !f.has('@') && !f.has(' '), "lettering: characters the game does not have are reported missing");
	}

	// ---------------- one compressed picture ----------------
	{
		picture p = read_picture(rom, maker::addr(0x3000));
		bool ok = p.w == 12 && p.h == 3;
		for (int y = 0; y < 3 && ok; y++)
			for (int x = 0; x < 12 && ok; x++)
				ok = p.at(x, y) == ((x < 2 || x > 10) ? 0u : rgb_of(0x1001 + unsigned((x - 2) % 3)));
		CHECK(ok, "pictures: the blitter's row compression (pixels left out at both ends) is undone");
		CHECK(read_picture(rom, maker::addr(0x3000) + 4).empty() && read_picture(rom, 0x1234).empty() && read_picture(rom, maker::addr(0x1fff0)).empty(),
		      "pictures: an address that is not a picture header gives nothing");
	}

	// ---------------- logos ----------------
	CHECK(s.logos.empty(), "logos: a program revision that is not listed gets none, even though it holds tables that look like one");
	{
		std::vector<picture> logos = read_logos(rom, 0x9000, 27);
		bool own = logos.size() == 27;
		for (int k = 0; k < 27 && own; k++) own = logos[size_t(k)].w == 12 && logos[size_t(k)].h == 10 && logos[size_t(k)].at(5, 5) == rgb_of(0x2001 + unsigned(k) * 2);
		CHECK(own, "logos: a table of 27 at its listed place is read, each logo in its own palette, in table order");
		CHECK(read_logos(rom, 0x9000, 28).empty() && read_logos(rom, 0x9002, 27).empty() && read_logos(rom, 0x8000, 27).empty() && read_logos(rom, 0x1fffe, 27).empty(),
		      "logos: a place that does not hold that many logo pictures gives none");
		s_logos = logos;
	}

	// ---------------- backgrounds ----------------
	CHECK(s.backgrounds.size() == 1, "backgrounds: the screen-sized module is found (" + std::to_string(s.backgrounds.size()) + " found)");
	if (s.backgrounds.size() == 1)
	{
		const picture &b = s.backgrounds[0];
		CHECK(b.w == 400 && b.h == 254, "backgrounds: the picture has the module's size");
		CHECK(b.at(0, 0) == c3 && b.at(1, 0) == c1 && b.at(1, 1) == c2 && b.at(7, 3) == c2, "backgrounds: a block is drawn where the list puts it");
		CHECK(b.at(107, 50) == c3 && b.at(100, 50) == c1 && b.at(106, 51) == c2, "backgrounds: a mirrored block is mirrored");
		CHECK(b.at(200, 200) == 0xff000000 && b.at(8, 0) == 0xff000000, "backgrounds: the rest is black, not see-through");
	}

	// ---------------- bad input: the fallback, never a crash ----------------
	{
		roms none;
		CHECK(read(none).empty(), "fallback: no ROM gives no art");
		roms cut = rom;
		cut.program.resize(0x8100);                       // ends inside the font table
		art_set a = read(cut);
		CHECK(a.fonts.empty() && a.logos.empty() && a.backgrounds.empty(), "fallback: a program ROM cut short gives no art");
		roms nogfx = rom;
		nogfx.gfx.assign(0x400, 0);                       // the pictures' pixels are gone
		CHECK(read(nogfx).empty(), "fallback: pictures whose pixels are outside the graphics ROM are skipped");
		roms noise;
		noise.program.resize(0x100000); noise.gfx.resize(0x40000);
		uint32_t x = 12345;
		for (auto &b : noise.program) { x = x * 1664525u + 1013904223u; b = uint8_t(x >> 24); }
		for (auto &b : noise.gfx) { x = x * 1664525u + 1013904223u; b = uint8_t(x >> 24); }
		art_set n = read(noise);
		CHECK(n.fonts.empty() && n.logos.empty(), "fallback: a megabyte of noise is read without a crash and gives no lettering or logos");
		roms wolf = rom;
		wolf.wolf = true;                                 // the wrong board: addresses mean something else
		read(wolf);
		CHECK(true, "fallback: the wrong board for the ROM is survived");
		game_catalog empty("arttest-no-such-folder");
		bool any = false;
		for (const game_info &g : game_list()) { roms r; any = any || empty.art_roms(g, r) || !r.program.empty(); }
		CHECK(!any, "fallback: with the ROMs taken away the catalogue hands the reader nothing, for every game");
	}

	// ---------------- the test screen ----------------
	{
		s.logos = s_logos;                               // as a listed revision would have them
		CHECK(ui::art_pages(s) == 3 && ui::art_pages(art_set()) == 1, "test screen: a page each for lettering, logos and every background; one page without ROM art");
		canvas blank(ui::screen::kW, ui::screen::kH), fb(ui::screen::kW, ui::screen::kH);
		ui::draw_art_page(fb, th, "NBA JAM Tournament Edition", art_set(), 0, ui::device::keyboard);
		write_png(out, "art_fallback.png", fb);
		bool title_colours = false;
		for (int y = 44; y < 62; y++) for (int x = 0; x < fb.w; x++) { uint32_t c = fb.px[size_t(y) * fb.w + x]; title_colours = title_colours || (((c >> 16) & 0xff) > 240 && (c & 0xff) < 140 && ((c >> 8) & 0xff) > 200); }
		CHECK(title_colours, "test screen: without ROM art the page is drawn in the project lettering");
		bool pages_differ = true;
		std::vector<uint32_t> last;
		for (int p = 0; p < 3; p++)
		{
			canvas cv(ui::screen::kW, ui::screen::kH);
			ui::draw_art_page(cv, th, "Made-up game", s, p, ui::device::xbox);
			write_png(out, "art_synthetic_page" + std::to_string(p + 1) + ".png", cv);
			pages_differ = pages_differ && cv.px != last && cv.px != fb.px;
			last = cv.px;
		}
		CHECK(pages_differ, "test screen: every page of a game with ROM art shows something else");
		canvas far(ui::screen::kW, ui::screen::kH);
		ui::draw_art_page(far, th, "Made-up game", s, 99, ui::device::xbox);
		ui::draw_picture(far, s.logos.empty() ? picture() : s.logos[0], -5, 295);
		ui::draw_picture(far, picture(), 10, 10);
		CHECK(true, "test screen: a page number out of range and pictures over the edge are survived");
		(void)blank;
	}

	// ---------------- U2.3: a theme made from ROM art ----------------
	{
		using namespace ui;
		const std::string file = "name = Made up" + std::string(1, char(10)) + "bar = #40c0ff" + std::string(1, char(10)) + "background = 400x254" + std::string(1, char(10)) + "background_dim = 0" + std::string(1, char(10)) +
		                         "title_font = 5" + std::string(1, char(10)) + "list_font = 5" + std::string(1, char(10));
		theme_spec sp = parse_theme("made", file);
		theme t = make_theme(sp, &s);
		const size_t W = screen::kW, H = screen::kH;
		// the 400 x 254 picture sits in the middle of the 400 x 300 screen: its row 0 is row 23
		CHECK(t.bar == 0xff40c0ff && t.backdrop.size() == W * H && t.backdrop[23 * W + 0] == c3 && t.backdrop[23 * W + 1] == c1 && t.backdrop[(23 + 50) * W + 107] == c3,
		      "themes: the ROM background of the size the file names is put behind the screen");
		CHECK(t.backdrop[0] != 0xff000000 && t.backdrop[(H - 1) * W] != 0xff000000 && t.backdrop[150 * W + 200] == 0xff000000,
		      "themes: above and below the picture the theme's gradient shows");
		CHECK(t.title_font && t.list_font && t.title_font->cap_h == 5 && t.list_font->has('A'), "themes: the ROM fonts of the heights the file names are used for titles and rows");
		theme_spec dim = parse_theme("made", file + "background_dim = 255" + std::string(1, char(10)) + "background_crop = 0 0 8 4" + std::string(1, char(10)));
		theme td = make_theme(dim, &s);
		CHECK(td.backdrop[0] == 0xff000000 && td.backdrop[299 * W + 399] == 0xff000000, "themes: a cropped part is repeated over the whole screen, and fully dimmed is black");
		theme_spec tiled = parse_theme("made", file + "background_crop = 0 0 8 4" + std::string(1, char(10)));
		theme tt = make_theme(tiled, &s);
		CHECK(tt.backdrop[0] == c3 && tt.backdrop[8] == c3 && tt.backdrop[4 * W] == c3 && tt.backdrop[1] == c1 && tt.backdrop[5 * W + 9] == c2, "themes: the crop tiles from the top left corner");
		theme_spec other = parse_theme("made", "bar = #40c0ff" + std::string(1, char(10)) + "background = 640x480" + std::string(1, char(10)) + "title_font = 61" + std::string(1, char(10)) + "list_font = 22" + std::string(1, char(10)));
		theme to = make_theme(other, &s);
		CHECK(to.bar == 0xff40c0ff && to.backdrop.empty() && !to.title_font && !to.list_font, "themes: a background or font the ROM does not have, or one too tall for its place, is left out; the colours stay");
		screen g = make_gallery();
		canvas cv(screen::kW, screen::kH);
		g.draw(cv, t, device::xbox, 0);
		write_png(out, "art_synthetic_theme.png", cv);
		CHECK(cv.px[23 * W] == c3, "themes: the gallery is drawn over the ROM background");
	}

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
