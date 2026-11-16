// themes.h - a theme per game family (roadmap U2.3): one layout everywhere; the colours,
// the background and the lettering follow the game.
//
// A theme is a small text file of `key = value` lines (';' starts a comment):
//
//   name = NBA Hangtime          shown in the gallery
//   games = nbahangt             the games it is for (ids as in games.cpp)
//   title_top = #fff080          colours, as #rrggbb: title_top title_bottom outline bg_top
//   ...                          bg_bottom tab tab_sel panel panel_edge strip strip_text text
//                                text_focus text_off header bar value status dialog
//   panel_alpha = 215            how solid the panels are over the background, 0 to 255
//   background = 400x254         which ROM background: the first one of this size; none: the gradient
//   background_crop = 12 196 84 50   optional: only this part of it (x y w h), repeated over the screen
//   background_dim = 90          darkens it, 0 (as it is) to 255 (black)
//   title_font = 18              which ROM font for titles, by the height of its capitals; 0: project lettering
//   list_font = 9                the same for rows and tabs
//
// The five themes are built into the program in that format (builtin_theme). A file
// themes/<id>.theme in the data folder is read instead of the built-in one, so a theme can
// be changed without rebuilding. The file names what to take from the ROMs; it holds no
// game art.
//
// ROM art is only used when the game's ROMs gave some (rom_art.h). Without it the neutral
// theme is used whole, as the roadmap decides, so a missing ROM never leaves a half-themed
// screen.
#pragma once
#include "ui.h"
#include <string>
#include <vector>

namespace ui
{

struct theme_spec
{
	std::string id, name;
	std::vector<std::string> games;
	theme colours;                       // the colour fields only; art is added by make_theme
	int bg_w = 0, bg_h = 0;              // 0: no ROM background
	int crop[4] = { 0, 0, 0, 0 };        // w 0: the whole picture, centred
	int dim = 0;
	int title_font = 0, list_font = 0;
};

// ids of the built-in themes: neutral first
const std::vector<std::string> &theme_ids();
const char *builtin_theme(const std::string &id);       // the built-in file's text, or nullptr
std::string theme_id_for_game(const std::string &game_id);   // "neutral" for a game no theme names

// Reads a theme file's text. Lines it does not understand are listed in `problems` and
// skipped; everything else still applies. Missing keys keep the neutral theme's values.
theme_spec parse_theme(const std::string &id, const std::string &text, std::vector<std::string> *problems = nullptr);
// themes/<id>.theme under `data_dir` if it is there, else the built-in one
theme_spec load_theme_spec(const std::string &id, const std::string &data_dir);

// The theme the screens draw with. `art` is what rom_art read from the game's ROMs; with
// none (nullptr or empty) the result is the neutral theme.
theme make_theme(const theme_spec &spec, const rom_art::art_set *art);

// Cross-fade between two themes, t from 0 (all a) to 1 (all b): colours and backgrounds
// mix, the lettering changes over at the middle.
theme mix_themes(const theme &a, const theme &b, float t);
constexpr unsigned kThemeFadeMs = 250;

} // namespace ui
