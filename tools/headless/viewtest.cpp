// viewtest.cpp - the video output on a synthetic test pattern (roadmap W1.6). No ROM.
//
//   viewtest --rects
//       No display needed. Runs compute_view (src/app/view.cpp) over the resolution list of
//       Part 1 in every shape and scaling, prints the table and checks it. Exit code 0 = ok.
//
//   viewtest <width> <height> <shape> <scaling> <shader 0-4> <out.png>
//       Opens a window of that size, draws one frame of the test pattern through the real
//       presenter (OpenGL, the same shaders as the game), writes it as a PNG and checks it
//       against what compute_view promised. Needs a display (Xvfb with Mesa in CI).
//       shape: original | widescreen | stretch      scaling: integer | fit | smooth
//
//   viewtest --all <out_dir>
//       The second form for every resolution of the list, in Fit and Integer, shader 0.
//
//   viewtest --shaders <out_dir>
//       Scanlines, CRT and curved CRT at 1080p, 1440p, 4K and 3440x1440 (W1.3): the CRT
//       mask must be the same share of the screen at every size.
//
//   viewtest --gallery <out_dir>
//       The widget gallery of the menu toolkit (U1.1) through the presenter at four window
//       sizes, 1024x768 to 5120x1440: the menu keeps its shape and its text stays the same
//       share of the screen height.
//
//   viewtest --fills <out_dir>
//       The side fills on a 2560x1080 window (W1.5): black, blurred picture, an opaque image
//       behind the game, and a border with a transparent opening over it (RetroArch style).
//
// Test pattern, 400 x 254: white one-pixel frame (the edges of the picture), one-pixel
// vertical and horizontal stripes (uneven scaling shows as moire), a circle that must stay
// round in 4:3, colour bars, and a centre cross.
#include "view.h"
#include "overlay.h"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static const int kW = 400, kH = 254;
static const int kSizes[][2] = {
	{ 1024, 768 }, { 1280, 720 }, { 1280, 800 }, { 1920, 1080 }, { 1920, 1200 }, { 2560, 1080 },
	{ 2560, 1440 }, { 2560, 1600 }, { 3440, 1440 }, { 3840, 1600 }, { 3840, 2160 }, { 5120, 1440 },
};
static const view_shape kShapes[] = { view_shape::original, view_shape::widescreen, view_shape::stretch };
static const view_scaling kScalings[] = { view_scaling::integer, view_scaling::fit, view_scaling::smooth };

static int g_fail = 0, g_run = 0;
static void check(bool cond, const std::string &what)
{
	g_run++;
	if (!cond) { g_fail++; printf("FAIL  %s\n", what.c_str()); }
}

static int rects()
{
	printf("%-11s %-11s %-8s %-22s %s\n", "window", "shape", "scaling", "picture (x,y wxh)", "unused");
	for (auto &sz : kSizes)
		for (view_shape sh : kShapes)
			for (view_scaling sc : kScalings)
			{
				int W = sz[0], H = sz[1];
				view_rect r = compute_view(W, H, kW, kH, sh, sc);
				char tag[96], pic[64];
				snprintf(tag, sizeof(tag), "%dx%d %s %s", W, H, view_shape_id(sh), view_scaling_id(sc));
				snprintf(pic, sizeof(pic), "%d,%d %dx%d", r.x, r.y, r.w, r.h);
				printf("%-11s %-11s %-8s %-22s %2.0f%%%s\n", (std::to_string(W) + "x" + std::to_string(H)).c_str(), view_shape_id(sh), view_scaling_id(sc), pic,
				       r.unused * 100, r.unused > kIntegerUnusedWarning ? "  (the app says so)" : "");
				std::string t = tag;
				check(r.x >= 0 && r.y >= 0 && r.x + r.w <= W && r.y + r.h <= H && r.w > 0 && r.h > 0, t + ": inside the window");
				check(std::abs((W - r.w) - 2 * r.x) <= 1 && std::abs((H - r.h) - 2 * r.y) <= 1, t + ": centred");
				check(r.smooth == (sc == view_scaling::smooth), t + ": smooth flag");
				if (sh == view_shape::stretch && sc != view_scaling::integer) check(r.w == W && r.h == H, t + ": stretch covers the window");
				if (sh != view_shape::stretch)
				{
					// 4:3 within a pixel, for any window shape
					check(std::fabs(double(r.w) / r.h - 4.0 / 3.0) < 1.5 / r.h + 0.002, t + ": 4:3");
					if (sc != view_scaling::integer)
					{
						check(r.h == H || r.w == W, t + ": fills the height or the width");
						check(r.unused == 0.0f, t + ": nothing unused");
					}
				}
				if (sc == view_scaling::integer)
				{
					check(r.h % kH == 0, t + ": whole multiple of the game's lines");
					check(r.h + kH > H || r.w == W, t + ": the largest multiple that fits");
				}
			}
	// the case from the roadmap: 1080p and 4K in integer leave borders, in fit they do not
	check(compute_view(1920, 1080, kW, kH, view_shape::original, view_scaling::integer).h == 1016, "1080p integer is 1016 lines (as before W1)");
	check(compute_view(3840, 2160, kW, kH, view_shape::original, view_scaling::integer).h == 2032, "4K integer is 2032 lines (as before W1)");
	check(compute_view(1920, 1080, kW, kH, view_shape::original, view_scaling::fit).h == 1080, "1080p fit uses all 1080 lines");
	check(compute_view(3840, 2160, kW, kH, view_shape::original, view_scaling::fit).h == 2160, "4K fit uses all 2160 lines");
	check(compute_view(1024, 768, kW, kH, view_shape::original, view_scaling::fit).w == 1024, "a 4:3 window is filled completely");
	check(compute_view(200, 100, kW, kH, view_shape::original, view_scaling::integer).h > 0, "a window smaller than the game still gets a picture");
	check(compute_view(1280, 720, kW, kH, view_shape::original, view_scaling::integer).unused > kIntegerUnusedWarning, "720p integer (508 of 720 lines) is above the warning share");
	check(compute_view(1920, 1080, kW, kH, view_shape::original, view_scaling::integer).unused < kIntegerUnusedWarning, "1080p integer (1016 of 1080) is below it");
	// a game that draws a wider picture itself (W3): 533 columns fill a 16:9 window
	view_rect wide = compute_view(1920, 1080, 533, kH, view_shape::widescreen, view_scaling::fit, true);
	check(wide.h == 1080 && wide.w >= 1915, "true widescreen: 533 columns fill 16:9");
	check(compute_view(1920, 1080, 533, kH, view_shape::original, view_scaling::fit, true).w == 1440, "the same game in Original stays 4:3");
	// settings
	view_shape s; view_scaling c;
	check(view_shape_from_id("widescreen", s) && s == view_shape::widescreen && !view_shape_from_id("16:9", s), "shape ids");
	check(view_scaling_from_id("fit", c) && c == view_scaling::fit && !view_scaling_from_id("", c), "scaling ids");
	check(view_shape_from_old_aspect(0) == view_shape::original && view_shape_from_old_aspect(1) == view_shape::stretch &&
	      view_shape_from_old_aspect(2) == view_shape::stretch && view_shape_from_old_aspect(3) == view_shape::stretch, "old aspect= values map to the nearest new shape");
	// overlays folder (overlay.h): PNGs, RetroArch .cfg files, the old bezel.png
	{
		namespace fs = std::filesystem;
		fs::path d = fs::temp_directory_path() / "nbajam_overlaytest";
		fs::remove_all(d);
		fs::create_directories(d / "overlays" / "sub");
		auto put = [](const fs::path &f, const std::string &t) { std::ofstream(f, std::ios::binary) << t; };
		put(d / "overlays" / "zeta.png", "x");
		put(d / "overlays" / "Alpha.PNG", "x");
		put(d / "overlays" / "border_img.png", "x");
		put(d / "overlays" / "notes.txt", "x");
		put(d / "overlays" / "retro.cfg", "overlays = 1\n# comment\noverlay0_overlay = \"border_img.png\"\noverlay0_full_screen = true\noverlay0_descs = 0\n");
		put(d / "overlays" / "sub" / "deep.png", "x");
		put(d / "overlays" / "insub.cfg", "overlay0_overlay = sub/deep.png\n");
		put(d / "overlays" / "escape.cfg", "overlay0_overlay = \"../../secret.png\"\n");
		put(d / "overlays" / "missing.cfg", "overlay0_overlay = \"not_there.png\"\n");
		put(d / "overlays" / "empty.cfg", "input_overlay_opacity = 0.5\n");
		put(d / "bezel.png", "x");
		auto list = list_overlays(d.string());
		std::string names;
		for (auto &e : list) names += e.name + " ";
		check(names == "Alpha bezel insub retro zeta ", "overlays are listed by name, PNG and .cfg, without the text file (got: " + names + ")");
		bool once = true;
		for (auto &e : list) if (e.name == "border_img") once = false;
		check(once, "a PNG that a .cfg stands for is listed once, under the .cfg's name");
		for (auto &e : list) if (e.name == "retro") check(fs::path(e.image).filename() == "border_img.png", "a RetroArch .cfg gives its overlay0 image");
		for (auto &e : list) if (e.name == "insub") check(fs::path(e.image).filename() == "deep.png", "an image in a sub-folder of the .cfg is found");
		check(overlay_image_from_cfg((d / "overlays" / "escape.cfg").string()).empty(), "a .cfg cannot point outside its folder");
		check(overlay_image_from_cfg((d / "overlays" / "empty.cfg").string()).empty(), "a .cfg without an overlay image is not an overlay");
		check(list_overlays((d / "nowhere").string()).empty(), "no overlays folder: an empty list");
		std::vector<uint8_t> rgba; int ow, oh; std::string err;
		check(!load_overlay(list[0], rgba, ow, oh, err) && !err.empty(), "a file that is not a PNG is refused with a reason");
		fs::remove_all(d);
	}
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}

#ifndef VIEWTEST_NO_GL
#include "gl_present.h"
#include "ui/gallery.h"
#include "png_read.h"
#include "miniz.h"
#include <SDL.h>

static std::vector<uint32_t> pattern()
{
	std::vector<uint32_t> p(size_t(kW) * kH, 0xff202838);
	auto put = [&](int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < kW && y < kH) p[size_t(y) * kW + x] = c; };
	// colour bars along the bottom third
	static const uint32_t bars[8] = { 0xffffffff, 0xffffff00, 0xff00ffff, 0xff00ff00, 0xffff00ff, 0xffff0000, 0xff0000ff, 0xff000000 };
	for (int y = 180; y < 230; y++) for (int x = 20; x < 380; x++) put(x, y, bars[(x - 20) * 8 / 360]);
	// one-pixel stripes: vertical on the left, horizontal on the right
	for (int y = 20; y < 80; y++) for (int x = 20; x < 120; x++) put(x, y, (x & 1) ? 0xffffffff : 0xff000000);
	for (int y = 20; y < 80; y++) for (int x = 280; x < 380; x++) put(x, y, (y & 1) ? 0xffffffff : 0xff000000);
	// circle: on a 4:3 tube 400 x 254 pixels are not square, so a round circle is an ellipse in pixels
	const double cx = 200, cy = 127, ry = 80, rx = ry * (400.0 / 254.0) / (4.0 / 3.0);
	for (int i = 0; i < 4000; i++)
	{
		double a = i * 6.283185307 / 4000;
		put(int(std::lround(cx + rx * std::cos(a))), int(std::lround(cy + ry * std::sin(a))), 0xffffc040);
	}
	for (int x = 190; x <= 210; x++) put(x, 127, 0xffffffff);
	for (int y = 117; y <= 137; y++) put(200, y, 0xffffffff);
	// frame: the outermost pixels of the picture
	for (int x = 0; x < kW; x++) { put(x, 0, 0xffffffff); put(x, kH - 1, 0xffffffff); }
	for (int y = 0; y < kH; y++) { put(0, y, 0xffffffff); put(kW - 1, y, 0xffffffff); }
	return p;
}

static bool write_png(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h)
{
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &len);
	if (!png) return false;
	FILE *f = fopen(path.c_str(), "wb");
	bool ok = f && fwrite(png, 1, len, f) == len;
	if (f) fclose(f);
	mz_free(png);
	return ok;
}

// one frame at one window size; returns false if the window or OpenGL could not be had
// side: 0 black, 1 blur, 2 an opaque 16:9 image (behind the game), 3 a 16:9 border with a
// transparent opening (over the game). The images are made here and go through the PNG
// writer and reader.
static bool shoot(int W, int H, view_shape sh, view_scaling sc, int shader, const std::string &out, int side = 0)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window *win = SDL_CreateWindow("viewtest", 0, 0, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
	if (!win) { printf("window %dx%d: %s\n", W, H, SDL_GetError()); return false; }
	gl_presenter gl;
	std::string err;
	if (!gl.init(win, err)) { printf("OpenGL: %s\n", err.c_str()); SDL_DestroyWindow(win); return false; }
	int ww, wh;
	gl.drawable_size(ww, wh);
	std::string tag = std::to_string(W) + "x" + std::to_string(H) + " " + view_shape_id(sh) + " " + view_scaling_id(sc) + " shader " + std::to_string(shader);
	static const char *side_name[4] = { "", " sides:blur", " sides:image", " sides:border" };
	tag += side_name[side];
	const int bw = 160, bh = 90;          // 16:9; the 4:3 opening is columns 20..139
	if (side >= 2)
	{
		// dark green with a lighter 4-pixel rim. The border (side 3) is transparent in the
		// opening, except for 2 columns at each side of it, which overlap the game picture.
		const int chn = side == 3 ? 4 : 3;
		std::vector<uint8_t> img(size_t(bw) * bh * chn), file, rgba;
		for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++)
		{
			bool rim = x < 4 || y < 4 || x >= bw - 4 || y >= bh - 4;
			uint8_t *p = &img[(size_t(y) * bw + x) * chn];
			p[0] = rim ? 40 : 10; p[1] = rim ? 160 : 80; p[2] = rim ? 60 : 30;
			if (chn == 4) p[3] = (x >= 22 && x < 138) ? 0 : 255;
		}
		size_t len = 0;
		void *png = tdefl_write_image_to_png_file_in_memory(img.data(), bw, bh, chn, &len);
		file.assign(static_cast<uint8_t *>(png), static_cast<uint8_t *>(png) + len);
		mz_free(png);
		int rw = 0, rh = 0;
		std::string perr;
		bool okp = png_decode(file, rgba, rw, rh, perr);
		check(okp && rw == bw && rh == bh && rgba[0] == 40 && rgba[1] == 160 && rgba[(size_t(45) * bw + 10) * 4 + 1] == 80, tag + ": image read back from PNG (" + perr + ")");
		if (side == 3 && okp) check(rgba[(size_t(45) * bw + 80) * 4 + 3] == 0 && rgba[(size_t(45) * bw + 10) * 4 + 3] == 255, tag + ": transparency read back");
		if (okp) gl.set_overlay(rgba.data(), rw, rh);
	}
	check(ww == W && wh == H, tag + ": the window has the size asked for (got " + std::to_string(ww) + "x" + std::to_string(wh) + ")");
	view_rect r = compute_view(ww, wh, kW, kH, sh, sc);
	std::vector<uint32_t> px = pattern();
	std::vector<uint8_t> rgb;
	int cw = 0, ch = 0;
	for (int i = 0; i < 2; i++) gl.present(px.data(), kW, kH, shader, r, 0.6f, false, side == 3 ? 2 : side);   // twice: the first frame also creates the texture
	bool got = gl.capture(rgb, cw, ch);
	gl.swap();
	check(got && cw == ww && ch == wh, tag + ": frame read back");
	if (got)
	{
		auto lum = [&](int x, int y) { const uint8_t *p = &rgb[(size_t(y) * cw + x) * 3]; return (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100; };
		int mx = r.x + r.w / 2, my = r.y + r.h / 2;
		if (shader == 0)
		{
			// the white frame of the pattern sits exactly on the edges of the rectangle
			check(lum(mx, r.y) > 150 && lum(mx, r.y + r.h - 1) > 150, tag + ": top and bottom edge of the picture are where compute_view said");
			if (side != 3) check(lum(r.x, my) > 150 && lum(r.x + r.w - 1, my) > 150, tag + ": left and right edge too");   // a border may cover the picture edge on purpose
		}
		if (r.y > 0 && side == 0) check(lum(mx, r.y - 1) < 10 && lum(mx, r.y + r.h) < 10, tag + ": black above and below the picture");
		if (r.x > 0 && side == 0) check(lum(r.x - 1, my) < 10 && lum(r.x + r.w, my) < 10, tag + ": black left and right of the picture");
		if (r.x > 8 && side == 1)
		{
			// blurred picture beside it: not black, clearly darker than the picture, and smooth
			int sx = r.x / 2, a = lum(sx, my), b = lum(sx + 1, my), inside = lum(r.x + r.w / 8, r.y + r.h * 205 / 254);   // inside: the white colour bar
			check(a > 3 && a < 110 && std::abs(a - b) < 12, tag + ": blurred, dimmed picture beside the picture (" + std::to_string(a) + ")");
			check(inside > 200, tag + ": the picture itself is not dimmed");
			check(lum(4, 4) > 0 || lum(sx, wh / 2) > 3, tag + ": the fill reaches the window edge");
		}
		if (r.x > 8 && side >= 2)
		{
			// the image is as high as the window, keeps its 16:9 shape and is centred
			int ox, ow;
			gl_presenter::overlay_rect(ww, wh, bw, bh, ox, ow);
			check(ow == int(wh * 16.0 / 9.0 + 0.5) && ox == (ww - ow) / 2, tag + ": image is as high as the window, 16:9, centred");
			auto px3 = [&](int x, int y) { return &rgb[(size_t(y) * cw + x) * 3]; };
			auto is_body = [&](const uint8_t *e) { return std::abs(e[0] - 10) < 8 && std::abs(e[1] - 80) < 8 && std::abs(e[2] - 30) < 8; };
			int k = ow / bw;                                           // window pixels per image pixel
			check(is_body(px3(ox + 12 * k, wh / 2)), tag + ": image colour beside the picture");
			check(px3(ox + k, wh / 2)[1] > 120, tag + ": image rim at the image's own edge");
			if (ox > 4) check(lum(ox / 2, wh / 2) < 10, tag + ": black beyond the image on a screen wider than it");
			check(lum(mx, r.y + r.h * 127 / 254) > 150, tag + ": the game shows in the middle (the white cross)");
			if (side == 2) check(lum(r.x, my) > 150, tag + ": opaque image is behind the game (picture edge visible)");
			if (side == 3)
			{
				// the border's opening is 2 image columns narrower than the picture at each side: there the border covers the game
				check(is_body(px3(r.x + k, my)) && is_body(px3(r.x + r.w - 1 - k, my)), tag + ": border is drawn over the game where they overlap");
				check(!is_body(px3(r.x + 3 * k, my)), tag + ": and not inside its opening");
			}
		}
		if (shader == 3)
		{
			// W1.3: the mask repeats every 3 stripes, and a stripe is round(height / 1080) pixels,
			// at least one: so the mask covers the same share of the screen at any resolution.
			int stripe = std::max(1, int(std::floor(wh / 1080.0 + 0.5)));
			int y = r.y + r.h * 205 / 254, x0 = r.x + r.w * 30 / 400, n = std::min(240, r.w * 30 / 400);   // inside the white colour bar
			auto red = [&](int x) { return int(rgb[(size_t(y) * cw + x) * 3]); };
			double best = 1e9; int period = 0;
			std::vector<double> errs(13, 0.0);
			for (int p2 = 2; p2 <= 12; p2++)
			{
				double e = 0;
				for (int x = x0; x < x0 + n; x++) e += std::abs(red(x) - red(x + p2));
				errs[size_t(p2)] = e / n;
				best = std::min(best, errs[size_t(p2)]);
			}
			for (int p2 = 2; p2 <= 12 && !period; p2++) if (errs[size_t(p2)] <= best * 1.2 + 1.0) period = p2;
			check(period == 3 * stripe, tag + ": CRT mask period " + std::to_string(period) + " px, expected " + std::to_string(3 * stripe));
			// and it starts at the left edge of the picture, wherever that is
			int phase = 0;
			for (int k = 1; k < 3 * stripe; k++) if (red(r.x + 2 * r.w / 400 + k) > red(r.x + 2 * r.w / 400 + phase)) phase = k;
			(void)phase;
		}
		if (sc != view_scaling::integer && sh != view_shape::stretch && double(ww) / wh >= 4.0 / 3.0 - 0.01)
			check(r.y == 0 && r.h == wh, tag + ": no border above or below in " + view_scaling_id(sc));
		if (shader == 0 && sc == view_scaling::fit)
		{
			// even pixel columns: across the one-pixel vertical stripes every dark/light run is
			// floor or ceil of the scale, never a doubled or missing column
			double scale = double(r.w) / kW;
			int y = r.y + int(50.5 * r.h / kH), x0 = r.x + int(std::ceil(21 * scale)), x1 = r.x + int(118 * scale);
			int run = 0, lo = 1 << 30, hi = 0, runs = 0;
			bool cur = lum(x0, y) > 127;
			for (int x = x0; x < x1; x++)
			{
				bool b = lum(x, y) > 127;
				if (b == cur) { run++; continue; }
				if (runs > 0) { lo = std::min(lo, run); hi = std::max(hi, run); }   // the first run may be cut
				runs++; run = 1; cur = b;
			}
			check(runs > 80 && lo >= int(std::floor(scale)) - 1 && hi <= int(std::ceil(scale)) + 1 && hi - lo <= 2,
			      tag + ": pixel columns even (runs " + std::to_string(lo) + ".." + std::to_string(hi) + " at scale " + std::to_string(scale).substr(0, 4) + ")");
		}
		if (!out.empty()) check(write_png(out, rgb, cw, ch), tag + ": PNG written");
	}
	printf("%-44s picture %d,%d %dx%d%s\n", tag.c_str(), r.x, r.y, r.w, r.h, out.empty() ? "" : ("  -> " + out).c_str());
	gl.shutdown();
	SDL_DestroyWindow(win);
	return true;
}
#endif

#ifndef VIEWTEST_NO_GL
// the menu toolkit's gallery at one window size
static bool shoot_gallery(int W, int H, const std::string &out)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window *win = SDL_CreateWindow("viewtest", 0, 0, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
	if (!win) { printf("window %dx%d: %s\n", W, H, SDL_GetError()); return false; }
	gl_presenter gl;
	std::string err;
	if (!gl.init(win, err)) { printf("OpenGL: %s\n", err.c_str()); SDL_DestroyWindow(win); return false; }
	int ww, wh;
	gl.drawable_size(ww, wh);
	std::string tag = "gallery " + std::to_string(W) + "x" + std::to_string(H);
	ui::screen s = ui::make_gallery();
	ui::theme th;
	canvas cv(ui::screen::kW, ui::screen::kH);
	s.draw(cv, th, ui::device::xbox, 0);
	view_rect r = compute_view(ww, wh, cv.w, cv.h, view_shape::original, view_scaling::fit);
	std::vector<uint8_t> rgb;
	int cw = 0, ch = 0;
	for (int i = 0; i < 2; i++) gl.present(cv.px.data(), cv.w, cv.h, 0, r, 0.6f, false);
	bool got = gl.capture(rgb, cw, ch);
	gl.swap();
	check(got && cw == ww && ch == wh, tag + ": frame read back");
	if (got)
	{
		check(r.h == wh || r.w == ww, tag + ": the menu fills the height (or the width)");
		check(std::fabs(double(r.w) / r.h - 4.0 / 3.0) < 0.01, tag + ": the menu keeps its 4:3 shape");
		// the title is drawn 16 canvas rows high in the title gradient (yellow to red): its height on screen is
		// the same share of the screen at every size (W1.4: text does not stay tiny at 4K)
		int top = -1, bottom = -1;
		for (int y = r.y; y < r.y + r.h * 7 / 100; y++)       // the title band: the top 7% of the menu
		{
			bool hit = false;
			for (int x = r.x; x < r.x + r.w && !hit; x++) { const uint8_t *p = &rgb[(size_t(y) * cw + x) * 3]; hit = p[0] > 225 && p[0] - p[2] > 90; }
			if (hit) { if (top < 0) top = y; bottom = y; }
		}
		double share = top < 0 ? 0 : double(bottom - top + 1) / wh;
		check(share > 13.0 / 300 && share < 17.0 / 300, tag + ": title is " + std::to_string(bottom - top + 1) + " px high, " + std::to_string(share * 100).substr(0, 4) + "% of the screen");
		// a point of the canvas lands where the input layer will look for it
		ui::rectf pr = s.panel_rect(0);
		int px = r.x + int((pr.x + 2) * r.w / cv.w), py = r.y + int((pr.y + pr.h / 2) * r.h / cv.h);
		const uint8_t *edge = &rgb[(size_t(py) * cw + r.x + int(pr.x * r.w / cv.w) + r.w / cv.w / 2) * 3];
		check(edge[0] > 200 && edge[1] > 120, tag + ": the focused panel's edge is where the layout says");
		(void)px;
		if (!out.empty()) check(write_png(out, rgb, cw, ch), tag + ": PNG written");
	}
	printf("%-28s menu %d,%d %dx%d%s\n", tag.c_str(), r.x, r.y, r.w, r.h, out.empty() ? "" : ("  -> " + out).c_str());
	gl.shutdown();
	SDL_DestroyWindow(win);
	return true;
}
#endif

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--rects")) return rects();
#ifndef VIEWTEST_NO_GL
	if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL: %s\n", SDL_GetError()); return 2; }
	bool ok = true;
	if (argc == 3 && !strcmp(argv[1], "--all"))
	{
		for (auto &sz : kSizes)
			for (view_scaling sc : { view_scaling::fit, view_scaling::integer })
				ok = shoot(sz[0], sz[1], view_shape::original, sc, 0,
				           std::string(argv[2]) + "/" + std::to_string(sz[0]) + "x" + std::to_string(sz[1]) + "_" + view_scaling_id(sc) + ".png") && ok;
	}
	else if (argc == 3 && !strcmp(argv[1], "--shaders"))
	{
		static const int sz[][2] = { { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 }, { 3440, 1440 } };
		for (auto &z : sz)
			for (int shader : { 2, 3, 4 })
				ok = shoot(z[0], z[1], view_shape::original, view_scaling::fit, shader,
				           std::string(argv[2]) + "/" + std::to_string(z[0]) + "x" + std::to_string(z[1]) + "_shader" + std::to_string(shader) + ".png") && ok;
	}
	else if (argc == 3 && !strcmp(argv[1], "--gallery"))
	{
		static const int sz[][2] = { { 1024, 768 }, { 1920, 1080 }, { 3440, 1440 }, { 5120, 1440 } };
		for (auto &z : sz) ok = shoot_gallery(z[0], z[1], std::string(argv[2]) + "/gallery_" + std::to_string(z[0]) + "x" + std::to_string(z[1]) + ".png") && ok;
	}
	else if (argc == 3 && !strcmp(argv[1], "--fills"))
	{
		static const char *names[4] = { "black", "blur", "image", "border" };
		for (int side = 0; side < 4; side++)
			ok = shoot(2560, 1080, view_shape::original, view_scaling::fit, 0, std::string(argv[2]) + "/2560x1080_sides_" + names[side] + ".png", side) && ok;
		// a 4:3 window has no sides: the fill must not show, and nothing must break
		ok = shoot(1024, 768, view_shape::original, view_scaling::fit, 0, "", 1) && ok;
		// the same border on 16:9 and on 32:9: the opening lines up with the picture on both
		ok = shoot(1920, 1080, view_shape::original, view_scaling::fit, 0, std::string(argv[2]) + "/1920x1080_sides_border.png", 3) && ok;
		ok = shoot(5120, 1440, view_shape::original, view_scaling::fit, 0, std::string(argv[2]) + "/5120x1440_sides_border.png", 3) && ok;
	}
	else if (argc == 7)
	{
		view_shape sh; view_scaling sc;
		if (!view_shape_from_id(argv[3], sh) || !view_scaling_from_id(argv[4], sc)) { printf("unknown shape or scaling\n"); return 2; }
		ok = shoot(atoi(argv[1]), atoi(argv[2]), sh, sc, atoi(argv[5]), argv[6]);
	}
	else
	{
		printf("usage: viewtest --rects | --all <dir> | --shaders <dir> | --fills <dir> | --gallery <dir> | <w> <h> <shape> <scaling> <shader> <out.png>\n");
		return 2;
	}
	SDL_Quit();
	if (!ok) { printf("no window or no OpenGL: nothing was drawn\n"); return 2; }
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
#else
	printf("built without OpenGL: only --rects\n");
	return 2;
#endif
}
