// rom_art.h - reads lettering, team logos, backgrounds and their palettes out of the user's
// own game ROMs for the menus (roadmap U2.2). Everything is decoded into memory when asked
// for; nothing is written to disk and no game art is in the program or the repository.
//
// Lettering and backgrounds are found in the program ROM by their shape, with no addresses:
//   lettering    the games' ASCII font tables: 128 pointers, none for the control codes,
//                the lower-case letters pointing at the same pictures as the capitals
//   backgrounds  the background modules: a size, a block count and three pointers (block
//                list, picture headers, palettes) whose contents all check out; only the
//                screen-sized ones are kept
// The team logo table is the exception: it cannot be told from other tables of the same
// make (players' faces, for one), so its place is listed per program revision in
// rom_art.cpp and checked before use; an unlisted revision gets no logos.
// A picture is: a header in the program ROM (width, height, anchor, address in the graphics
// ROM, blitter control word, palette) and its pixels in the graphics ROM, 1 to 8 bits each,
// with or without the blitter's row compression (see src/board/midway_video.cpp).
//
// Whatever does not check out is skipped without a message: the caller gets fewer pictures,
// or none, and uses the project's own art instead.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rom_art
{

// 0xAARRGGBB; alpha is 0 (see-through) or 255
struct picture
{
	int w = 0, h = 0;
	std::vector<uint32_t> px;
	bool empty() const { return px.empty(); }
	uint32_t at(int x, int y) const { return px[size_t(y) * size_t(w) + size_t(x)]; }
};

struct font
{
	int cap_h = 0;                       // height of the capital A
	picture glyph[128];                  // empty where the game has no such character
	std::vector<uint32_t> palette;       // the colours its letters use, index 0 see-through
	bool has(char c) const { unsigned u = static_cast<unsigned char>(c); return u < 128 && !glyph[u].empty(); }
	int count() const { int n = 0; for (auto &g : glyph) n += !g.empty(); return n; }
};

struct art_set
{
	std::vector<font> fonts;             // smallest first
	std::vector<picture> logos;          // in the game's own team order
	std::vector<picture> backgrounds;    // screen-sized, in ROM order
	bool empty() const { return fonts.empty() && logos.empty() && backgrounds.empty(); }
};

// The ROMs of one game as the boards see them: the program as bytes in address order (two
// per 16-bit word, low byte first), the graphics ROM as the blitter addresses it.
struct roms
{
	std::vector<uint8_t> program, gfx;
	bool wolf = false;                   // Wolf Unit: picture addresses start at 0 instead of 0x2000000
};

// Finds and decodes everything. Safe on any input: wrong, cut short or empty ROMs give an
// empty or partial set.
art_set read(const roms &r);

// A logo table at a given byte offset of the program: `count` pointers to compressed 8-bit
// pictures with a palette each. Empty unless every one checks out and decodes.
std::vector<picture> read_logos(const roms &r, uint32_t table_offset, int count);

// One picture by the address of its header, for the tests. Empty if it does not check out.
picture read_picture(const roms &r, uint32_t header_address);

// Program ROM addresses are bit addresses from here, as the games' own pointers are.
constexpr uint32_t kProgramBase = 0xff800000u;

} // namespace rom_art
