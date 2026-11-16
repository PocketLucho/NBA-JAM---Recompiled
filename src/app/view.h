// view.h - where the game picture goes inside the window (roadmap W1.1, W1.2).
// Pure arithmetic: no SDL and no OpenGL in here, so it is tested without a display.
//
// Shape (F7):
//   original    4:3, as the arcade tube showed it, centred
//   widescreen  true widescreen (more court, not a stretched picture) on the games that
//               support it; until those arrive (W3) and on every other game: 4:3 centred
//   stretch     the picture is stretched to the whole window, whatever its shape
// The target shape always comes from the window, never from a fixed list of ratios.
//
// Scaling (F5):
//   integer     height is the largest whole multiple of the game's lines that fits
//   fit         the picture fills the height (or the width, in a narrow window) exactly;
//               pixels are drawn at a whole-number size and only their edges are blended
//   smooth      the same rectangle, plain bilinear filtering
#pragma once

enum class view_shape { original, widescreen, stretch };
enum class view_scaling { integer, fit, smooth };

struct view_rect
{
	int x = 0, y = 0, w = 0, h = 0;   // destination rectangle in window pixels (y from the top)
	bool smooth = false;              // plain bilinear instead of crisp pixels
	// share of the window that the picture could have covered in "fit" but does not
	// (0..1). Only "integer" can leave any; the app says so when it is large.
	float unused = 0.0f;
};

constexpr float kOriginalAspect = 4.0f / 3.0f;
// "integer" tells the player when it leaves more than this share unused
constexpr float kIntegerUnusedWarning = 0.15f;

// src_w x src_h: the game picture in pixels (400 x 254 today). true_wide: the running game
// draws a wider picture itself (W3); its src_w already is that wider picture.
view_rect compute_view(int win_w, int win_h, int src_w, int src_h, view_shape shape, view_scaling scaling, bool true_wide = false);

const char *view_shape_name(view_shape s);       // for messages and the settings file
const char *view_scaling_name(view_scaling s);
const char *view_shape_id(view_shape s);         // "original", "widescreen", "stretch"
const char *view_scaling_id(view_scaling s);     // "integer", "fit", "smooth"
bool view_shape_from_id(const char *id, view_shape &out);
bool view_scaling_from_id(const char *id, view_scaling &out);
// settings written by versions before W1: aspect=0..3 (4:3, 16:9 stretched, 16:10 stretched, fill)
view_shape view_shape_from_old_aspect(int aspect);
