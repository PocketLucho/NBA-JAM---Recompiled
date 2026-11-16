// png_read.cpp - see png_read.h. Inflate comes from miniz.
#include "png_read.h"
#include "miniz.h"
#include <cstdlib>
#include <cstring>

static uint32_t be32(const uint8_t *p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

bool png_decode(const std::vector<uint8_t> &f, std::vector<uint8_t> &rgba, int &w, int &h, std::string &err)
{
	static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (f.size() < 8 + 25 || memcmp(f.data(), sig, 8) != 0) { err = "not a PNG file"; return false; }
	int depth = 0, type = 0, interlace = 0;
	std::vector<uint8_t> idat, pal, trns;
	w = h = 0;
	for (size_t p = 8; p + 12 <= f.size();)
	{
		uint32_t len = be32(&f[p]);
		const uint8_t *tag = &f[p + 4], *data = &f[p + 8];
		if (len > f.size() || p + 12 + len > f.size()) { err = "PNG is cut off"; return false; }
		if (!memcmp(tag, "IHDR", 4) && len >= 13)
		{
			w = int(be32(data)); h = int(be32(data + 4));
			depth = data[8]; type = data[9]; interlace = data[12];
		}
		else if (!memcmp(tag, "PLTE", 4)) pal.assign(data, data + len);
		else if (!memcmp(tag, "tRNS", 4)) trns.assign(data, data + len);
		else if (!memcmp(tag, "IDAT", 4)) idat.insert(idat.end(), data, data + len);
		else if (!memcmp(tag, "IEND", 4)) break;
		p += 12 + size_t(len);
	}
	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { err = "PNG has no usable size"; return false; }
	if (depth != 8 || interlace != 0) { err = "only 8-bit, non-interlaced PNGs are read"; return false; }
	int ch = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : type == 6 ? 4 : 0;
	if (!ch || (type == 3 && pal.size() < 3)) { err = "PNG colour type not supported"; return false; }
	size_t stride = size_t(w) * ch, want = (stride + 1) * size_t(h), got = 0;
	void *raw = tinfl_decompress_mem_to_heap(idat.data(), idat.size(), &got, TINFL_FLAG_PARSE_ZLIB_HEADER);
	if (!raw || got < want) { if (raw) mz_free(raw); err = "PNG data is damaged"; return false; }
	uint8_t *d = static_cast<uint8_t *>(raw);
	// undo the per-row filters in place
	for (int y = 0; y < h; y++)
	{
		uint8_t *row = d + size_t(y) * (stride + 1) + 1, *up = y ? row - (stride + 1) : nullptr;
		int filter = row[-1];
		for (size_t i = 0; i < stride; i++)
		{
			int a = i >= size_t(ch) ? row[i - ch] : 0, b = up ? up[i] : 0, c = (up && i >= size_t(ch)) ? up[i - ch] : 0, v = 0;
			switch (filter)
			{
			case 0: v = 0; break;
			case 1: v = a; break;
			case 2: v = b; break;
			case 3: v = (a + b) / 2; break;
			case 4: { int pp = a + b - c, pa = abs(pp - a), pb = abs(pp - b), pc = abs(pp - c); v = (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c; break; }
			default: mz_free(raw); err = "PNG data is damaged"; return false;
			}
			row[i] = uint8_t(row[i] + v);
		}
	}
	rgba.resize(size_t(w) * h * 4);
	for (int y = 0; y < h; y++)
	{
		const uint8_t *row = d + size_t(y) * (stride + 1) + 1;
		uint8_t *o = &rgba[size_t(y) * w * 4];
		for (int x = 0; x < w; x++, o += 4)
		{
			const uint8_t *s = row + size_t(x) * ch;
			switch (type)
			{
			case 0: o[0] = o[1] = o[2] = s[0]; o[3] = 255; break;
			case 2: o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = 255; break;
			case 3:
			{
				size_t i = s[0];
				if (i * 3 + 2 < pal.size()) { o[0] = pal[i * 3]; o[1] = pal[i * 3 + 1]; o[2] = pal[i * 3 + 2]; } else o[0] = o[1] = o[2] = 0;
				o[3] = i < trns.size() ? trns[i] : 255;
				break;
			}
			case 4: o[0] = o[1] = o[2] = s[0]; o[3] = s[1]; break;
			default: o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = s[3]; break;
			}
		}
	}
	mz_free(raw);
	return true;
}
