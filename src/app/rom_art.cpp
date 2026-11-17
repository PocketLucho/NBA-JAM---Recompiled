// rom_art.cpp - see rom_art.h
#include "rom_art.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace rom_art
{

namespace
{

struct header { int w = 0, h = 0; uint32_t sag = 0; unsigned ctrl = 0; uint32_t pal = 0; };

struct reader
{
	const roms &r;
	const std::vector<uint8_t> &p;
	explicit reader(const roms &roms_) : r(roms_), p(roms_.program) {}

	// byte offset of a program address, or -1
	long off(uint32_t addr, size_t need = 2) const
	{
		if (addr < kProgramBase || (addr & 7)) return -1;
		size_t o = (addr - kProgramBase) / 8;
		return o + need <= p.size() ? long(o) : -1;
	}
	unsigned w16(size_t o) const { return unsigned(p[o]) | unsigned(p[o + 1]) << 8; }
	int s16(size_t o) const { return int16_t(w16(o)); }
	uint32_t l32(size_t o) const { return uint32_t(w16(o)) | uint32_t(w16(o + 2)) << 16; }

	// a palette: a count, then that many 15-bit colours
	bool palette(uint32_t addr, std::vector<uint32_t> *out = nullptr) const
	{
		long o = off(addr);
		if (o < 0) return false;
		unsigned n = w16(size_t(o));
		if (n == 0 || n > 256 || size_t(o) + 2 + n * 2 > p.size()) return false;
		if (out)
		{
			out->resize(n);
			for (unsigned i = 0; i < n; i++)
			{
				unsigned c = w16(size_t(o) + 2 + i * 2);
				auto ch = [&](int sh) { return uint32_t(((c >> sh) & 31) * 255 / 31); };
				(*out)[i] = 0xff000000 | ch(10) << 16 | ch(5) << 8 | ch(0);
			}
		}
		return true;
	}

	// a picture header: w, h, anchor x, anchor y (words), graphics address (long), control
	// (word), palette (long)
	bool head(uint32_t addr, header &h) const
	{
		long o = off(addr, 18);
		if (o < 0) return false;
		h.w = s16(size_t(o)); h.h = s16(size_t(o) + 2);
		h.sag = l32(size_t(o) + 8); h.ctrl = w16(size_t(o) + 12); h.pal = l32(size_t(o) + 14);
		return h.w > 0 && h.w <= 512 && h.h > 0 && h.h <= 400 && palette(h.pal) && pixels_in_rom(h);
	}

	bool pixels_in_rom(const header &h) const
	{
		uint32_t base = r.wolf ? 0u : 0x2000000u;
		if (h.sag < base) return false;
		return (uint64_t(h.sag - base) >> 3) + 3 < r.gfx.size();
	}

	// The pixels, as the blitter reads them: `bpp` bits each, rows one after the other with
	// no padding; with compression each row starts with a byte holding how many see-through
	// pixels were left out at its start and at its end.
	picture decode(const header &h, const std::vector<uint32_t> &pal) const
	{
		picture out;
		if (h.w <= 0 || h.h <= 0 || h.w > 1024 || h.h > 1024 || !pixels_in_rom(h)) return out;
		const std::vector<uint8_t> &g = r.gfx;
		const unsigned bpp = ((h.ctrl >> 12) & 7) ? ((h.ctrl >> 12) & 7) : 8;
		const bool packed = (h.ctrl & 0x80) != 0;
		uint64_t o = h.sag - (r.wolf ? 0u : 0x2000000u);
		auto bits = [&](uint64_t at, unsigned n, unsigned &v)
		{
			size_t b = size_t(at >> 3);
			if (b + 2 >= g.size()) return false;
			v = ((unsigned(g[b]) | unsigned(g[b + 1]) << 8 | unsigned(g[b + 2]) << 16) >> (at & 7)) & ((1u << n) - 1);
			return true;
		};
		out.w = h.w; out.h = h.h;
		out.px.assign(size_t(h.w) * size_t(h.h), 0);
		for (int y = 0; y < h.h; y++)
		{
			int pre = 0, post = 0;
			if (packed)
			{
				unsigned v;
				if (!bits(o, 8, v)) return picture();
				o += 8;
				pre = int(v & 15) << ((h.ctrl >> 8) & 3);
				post = int(v >> 4) << ((h.ctrl >> 10) & 3);
			}
			int n = h.w - pre - post;
			for (int x = 0; x < n; x++, o += bpp)
			{
				unsigned v;
				if (!bits(o, bpp, v)) return picture();
				if (v && v < pal.size() && pre + x < h.w) out.px[size_t(y) * size_t(h.w) + size_t(pre + x)] = pal[v];
			}
		}
		return out;
	}

	// ---------------------------------------------------------------- lettering
	void fonts(std::vector<font> &out) const
	{
		if (p.size() < 512) return;
		for (size_t o = 0; o + 512 <= p.size() && out.size() < 16; o += 2)
		{
			if (p[o] | p[o + 1] | p[o + 2] | p[o + 3]) continue;
			if (std::memcmp(&p[o + 0x41 * 4], &p[o + 0x61 * 4], 26 * 4) != 0) continue;
			bool ok = true;
			for (int c = 0; c <= 0x20 && ok; c++) ok = l32(o + size_t(c) * 4) == 0;
			header hd[128];
			std::map<uint32_t, int> pals;
			for (int c = 0x21; c < 128 && ok; c++)
			{
				uint32_t a = l32(o + size_t(c) * 4);
				hd[c] = header();
				if (a == 0) { ok = c < 'A' || c > 'Z'; continue; }
				ok = head(a, hd[c]) && hd[c].w <= 128 && hd[c].h <= 128;
				if (ok) pals[hd[c].pal]++;
			}
			if (!ok) continue;
			// one palette for the whole font: the one most of its letters name
			uint32_t pal = 0; int most = 0;
			for (auto &kv : pals) if (kv.second > most) { most = kv.second; pal = kv.first; }
			font f;
			if (!palette(pal, &f.palette)) continue;
			for (int c = 0x21; c < 128; c++) if (hd[c].w) f.glyph[c] = decode(hd[c], f.palette);
			if (f.glyph['A'].empty()) continue;
			f.cap_h = f.glyph['A'].h;
			out.push_back(std::move(f));
			o += 510;
		}
		std::stable_sort(out.begin(), out.end(), [](const font &a, const font &b) { return a.cap_h < b.cap_h; });
	}

	// ---------------------------------------------------------------- team logos
	bool logo_head(size_t o, header &h) const
	{
		return o + 4 <= p.size() && head(l32(o), h) && (h.ctrl & 0x7080) == 0x0080 && h.w >= 10 && h.h >= 10 && h.w <= 130 && h.h <= 130;
	}
	// `count` pointers to compressed 8-bit pictures, each with a palette of its own
	void logos(size_t table, int count, std::vector<picture> &out) const
	{
		std::vector<header> run;
		std::set<uint32_t> pals;
		header h;
		for (int i = 0; i < count; i++)
		{
			if (!logo_head(table + size_t(i) * 4, h) || !pals.insert(h.pal).second) return;
			run.push_back(h);
		}
		for (const header &x : run)
		{
			std::vector<uint32_t> pal;
			palette(x.pal, &pal);
			picture pic = decode(x, pal);
			if (pic.empty()) { out.clear(); return; }
			out.push_back(std::move(pic));
		}
	}

	// ---------------------------------------------------------------- backgrounds
	void backgrounds(std::vector<picture> &out) const
	{
		std::set<uint32_t> seen;
		for (size_t o = 0; o + 18 <= p.size() && out.size() < 12; o += 2)
		{
			unsigned xs = w16(o), ys = w16(o + 2), n = w16(o + 4);
			// screen-sized modules only (the screen is 400 x 254)
			if (xs < 380 || xs > 420 || ys < 236 || ys > 260 || n == 0 || n > 3000) continue;
			uint32_t blk = l32(o + 6);
			long bo = off(blk, size_t(n) * 8), ho = off(l32(o + 10), 10), po = off(l32(o + 14), 4);
			if (bo < 0 || ho < 0 || po < 0 || seen.count(blk)) continue;
			struct block { unsigned flags; int x, y; header h; };
			std::vector<block> blocks;
			bool ok = true;
			for (unsigned i = 0; i < n && ok; i++)
			{
				size_t b = size_t(bo) + i * 8;
				block k;
				k.flags = w16(b); k.x = s16(b + 2); k.y = s16(b + 4);
				unsigned hw = w16(b + 6), hi = hw & 0x0fff, pi = (k.flags & 15) | (hw >> 12) << 4;
				size_t hh = size_t(ho) + hi * 10, pp = size_t(po) + pi * 4;
				ok = hh + 10 <= p.size() && pp + 4 <= p.size();
				if (!ok) break;
				k.h.w = int(w16(hh)); k.h.h = int(w16(hh + 2)); k.h.sag = l32(hh + 4); k.h.ctrl = w16(hh + 8); k.h.pal = l32(pp);
				ok = k.h.w > 0 && k.h.w <= 1024 && k.h.h > 0 && k.h.h <= 512 && k.x >= -64 && k.x <= int(xs) && k.y >= -64 && k.y <= int(ys) &&
				     palette(k.h.pal) && pixels_in_rom(k.h);
				blocks.push_back(k);
			}
			if (!ok) continue;
			seen.insert(blk);
			// lowest layer first; blocks of one layer in list order
			std::stable_sort(blocks.begin(), blocks.end(), [](const block &a, const block &b) { return (a.flags >> 8) < (b.flags >> 8); });
			picture bg;
			bg.w = int(xs); bg.h = int(ys);
			bg.px.assign(size_t(xs) * ys, 0xff000000);
			std::map<uint32_t, std::vector<uint32_t>> pals;
			bool drawn = true;
			for (const block &k : blocks)
			{
				auto &pal = pals[k.h.pal];
				if (pal.empty()) palette(k.h.pal, &pal);
				picture pic = decode(k.h, pal);
				if (pic.empty()) { drawn = false; break; }
				const bool hflip = k.flags & 0x10, vflip = k.flags & 0x20;
				for (int y = 0; y < pic.h; y++)
					for (int x = 0; x < pic.w; x++)
					{
						uint32_t c = pic.at(hflip ? pic.w - 1 - x : x, vflip ? pic.h - 1 - y : y);
						int dx = k.x + x, dy = k.y + y;
						if (c && dx >= 0 && dy >= 0 && dx < bg.w && dy < bg.h) bg.px[size_t(dy) * size_t(bg.w) + size_t(dx)] = c;
					}
			}
			if (drawn) out.push_back(std::move(bg));
		}
	}
};

} // namespace

// Where the team logo table is, per program revision. The lettering and the backgrounds are
// told by their shape; the logos cannot be: the games hold other tables of the same make
// (on Hangtime one of exactly 29 players' faces), and a face must never be shown as a
// logo. So the place is listed for the revisions the program runs, by the CRC-32 of the
// program ROM as the board sees it, and still checked before use. These are offsets, not
// game data. An unlisted revision gets no logos.
struct logo_place { uint32_t crc; uint32_t table; int count; };
static const logo_place kLogos[] = {
	{ 0x13680337, 0x02aefe, 27 },   // NBA JAM rev 3.01
	{ 0x70646d2c, 0x03a370, 27 },   // NBA JAM T.E. rev 4.0
	{ 0x03f015c6, 0x03d288, 27 },   // NBA JAM T.E. Special Edition rev 5.02
	{ 0x6ae2feac, 0x058ea4, 29 },   // NBA Maximum Hangtime 1.3
	{ 0x6fd39793, 0x057d62, 29 },   // NBA Maximum Hangtime 1.03
	{ 0xc9692568, 0x058ea4, 29 },   // NBA Hangtime L1.3
	{ 0x1db94b44, 0x058e9c, 29 },   // NBA Hangtime L1.1
};

static uint32_t crc32(const std::vector<uint8_t> &d)
{
	static uint32_t tab[256];
	if (!tab[1])
		for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (c & 1 ? 0xedb88320u : 0); tab[i] = c; }
	uint32_t c = 0xffffffffu;
	for (uint8_t b : d) c = tab[(c ^ b) & 0xff] ^ (c >> 8);
	return ~c;
}

std::vector<picture> read_logos(const roms &r, uint32_t table_offset, int count)
{
	std::vector<picture> out;
	if (r.gfx.size() >= 8 && count > 0 && count <= 64) reader(r).logos(table_offset, count, out);
	return out;
}

art_set read(const roms &r)
{
	art_set s;
	if (r.program.size() < 1024 || r.gfx.size() < 1024) return s;
	reader rd(r);
	rd.fonts(s.fonts);
	const uint32_t crc = crc32(r.program);
	for (const logo_place &l : kLogos) if (l.crc == crc) rd.logos(l.table, l.count, s.logos);
	rd.backgrounds(s.backgrounds);
	return s;
}

picture read_picture(const roms &r, uint32_t header_address)
{
	reader rd(r);
	header h;
	std::vector<uint32_t> pal;
	if (r.gfx.size() < 8 || !rd.head(header_address, h) || !rd.palette(h.pal, &pal)) return picture();
	return rd.decode(h, pal);
}

} // namespace rom_art
