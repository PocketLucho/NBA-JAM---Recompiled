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
// Test pattern, 400 x 254: white one-pixel frame (the edges of the picture), one-pixel
// vertical and horizontal stripes (uneven scaling shows as moire), a circle that must stay
// round in 4:3, colour bars, and a centre cross.
#include "view.h"
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
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}

#ifndef VIEWTEST_NO_GL
#include "gl_present.h"
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
static bool shoot(int W, int H, view_shape sh, view_scaling sc, int shader, const std::string &out)
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
	check(ww == W && wh == H, tag + ": the window has the size asked for (got " + std::to_string(ww) + "x" + std::to_string(wh) + ")");
	view_rect r = compute_view(ww, wh, kW, kH, sh, sc);
	std::vector<uint32_t> px = pattern();
	std::vector<uint8_t> rgb;
	int cw = 0, ch = 0;
	for (int i = 0; i < 2; i++) gl.present(px.data(), kW, kH, shader, r, 0.6f, false);   // twice: the first frame also creates the texture
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
			check(lum(r.x, my) > 150 && lum(r.x + r.w - 1, my) > 150, tag + ": left and right edge too");
		}
		if (r.y > 0) check(lum(mx, r.y - 1) < 10 && lum(mx, r.y + r.h) < 10, tag + ": black above and below the picture");
		if (r.x > 0) check(lum(r.x - 1, my) < 10 && lum(r.x + r.w, my) < 10, tag + ": black left and right of the picture");
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
	else if (argc == 7)
	{
		view_shape sh; view_scaling sc;
		if (!view_shape_from_id(argv[3], sh) || !view_scaling_from_id(argv[4], sc)) { printf("unknown shape or scaling\n"); return 2; }
		ok = shoot(atoi(argv[1]), atoi(argv[2]), sh, sc, atoi(argv[5]), argv[6]);
	}
	else
	{
		printf("usage: viewtest --rects | viewtest --all <dir> | viewtest <w> <h> <shape> <scaling> <shader> <out.png>\n");
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
