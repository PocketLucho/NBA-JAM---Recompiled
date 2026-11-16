// view.cpp - see view.h
#include "view.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// largest rectangle of the given width:height ratio inside the window
static void best_fit(int win_w, int win_h, double aspect, int &w, int &h)
{
	w = win_w;
	h = int(std::lround(win_w / aspect));
	if (h > win_h) { h = win_h; w = int(std::lround(win_h * aspect)); }
	w = std::clamp(w, 1, win_w);
	h = std::clamp(h, 1, win_h);
}

view_rect compute_view(int win_w, int win_h, int src_w, int src_h, view_shape shape, view_scaling scaling, bool true_wide)
{
	view_rect r;
	win_w = std::max(win_w, 1); win_h = std::max(win_h, 1);
	src_w = std::max(src_w, 1); src_h = std::max(src_h, 1);
	r.smooth = scaling == view_scaling::smooth;

	int fw, fh;      // what "fit" gives: the reference for the unused share
	if (shape == view_shape::stretch) { fw = win_w; fh = win_h; }
	else
	{
		// The 400-column picture is a 4:3 picture. A game that draws more columns itself
		// (true widescreen) keeps the same pixel shape, so its picture is wider by that much.
		double aspect = kOriginalAspect;
		if (shape == view_shape::widescreen && true_wide) aspect = kOriginalAspect * src_w / 400.0;
		best_fit(win_w, win_h, aspect, fw, fh);
	}
	r.w = fw; r.h = fh;
	if (scaling == view_scaling::integer && fh >= src_h)
	{
		int n = fh / src_h;                       // whole lines per game line
		r.h = n * src_h;
		// the width follows the height, so the shape is kept; stretch keeps the full width
		if (shape != view_shape::stretch) r.w = std::min(win_w, int(std::lround(double(fw) * r.h / fh)));
	}
	r.x = (win_w - r.w) / 2;
	r.y = (win_h - r.h) / 2;
	r.unused = 1.0f - float(double(r.w) * r.h / (double(fw) * fh));
	if (r.unused < 0.0005f) r.unused = 0.0f;
	return r;
}

const char *view_shape_name(view_shape s)
{
	switch (s)
	{
	case view_shape::original: return "Original 4:3";
	case view_shape::widescreen: return "Widescreen";
	default: return "Stretch";
	}
}

const char *view_scaling_name(view_scaling s)
{
	switch (s)
	{
	case view_scaling::integer: return "Integer";
	case view_scaling::fit: return "Fit";
	default: return "Smooth";
	}
}

const char *view_shape_id(view_shape s) { return s == view_shape::original ? "original" : s == view_shape::widescreen ? "widescreen" : "stretch"; }
const char *view_scaling_id(view_scaling s) { return s == view_scaling::integer ? "integer" : s == view_scaling::fit ? "fit" : "smooth"; }

bool view_shape_from_id(const char *id, view_shape &out)
{
	for (view_shape s : { view_shape::original, view_shape::widescreen, view_shape::stretch })
		if (!strcmp(id, view_shape_id(s))) { out = s; return true; }
	return false;
}

bool view_scaling_from_id(const char *id, view_scaling &out)
{
	for (view_scaling s : { view_scaling::integer, view_scaling::fit, view_scaling::smooth })
		if (!strcmp(id, view_scaling_id(s))) { out = s; return true; }
	return false;
}

view_shape view_shape_from_old_aspect(int aspect)
{
	// 0 was 4:3. 1 and 2 stretched the picture to 16:9 and 16:10, 3 to the whole window:
	// all three are what is now called, honestly, Stretch.
	return aspect <= 0 ? view_shape::original : view_shape::stretch;
}
