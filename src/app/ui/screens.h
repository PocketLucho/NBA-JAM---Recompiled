// screens.h - the program's screens rebuilt on the menu toolkit (roadmap U3). No SDL and no
// emulator in here: main.cpp fills a plain description of what there is to show, these
// functions turn it into a toolkit screen and back, and the headless tests walk and draw
// the same screens.
//
//   U3.1  the game picker: the games in a list, greyed when they cannot be played; beside
//         it the highlighted game's name in its own lettering, its revision and, when it
//         cannot be played, why; under it the rest of the program.
//         With no game playable at all it says where the ROMs go.
//   U3.2  Settings: Video, Sound, Game and Program tabs. Everything on it is also on a key
//         (F4 to F8, F11, - and +), which keep working.
#pragma once
#include "ui.h"
#include <string>
#include <vector>

namespace ui
{

// ---------------------------------------------------------------- game picker (U3.1)
struct picker_game
{
	std::string title, revision, board;
	std::vector<std::string> big;        // the name in one or two short lines, for the large lettering
	bool available = false, from_source = false;
	std::string why;                     // when not available: what is missing
};

struct picker_state
{
	std::vector<picker_game> games;
	int mods_on = 0, mods_total = 0;
	bool news_on = true, news_new = false;
	std::string news_tag, version, roms_dir;
};

enum picker_id { pk_game0 = 1, pk_online = 50, pk_settings, pk_mods, pk_controls, pk_news, pk_about, pk_rescan, pk_quit };

screen make_picker(const picker_state &st);
// The game the screen is about: the focused one, or the one last focused while focus is in
// the menu under the list. `last` is kept by the caller between frames.
int picker_game_index(const screen &s, int last);
// Refreshes what depends on the state and on the highlighted game: the list's status
// column, the menu's values and the panel beside the list.
void update_picker(screen &s, const picker_state &st, int game);
// The highlighted game's name in large lettering at the top of the side panel: in the
// game's own font when the theme's art has one that fits, else the project lettering.
void draw_picker_name(canvas &cv, const screen &s, const theme &th, const picker_game &g, const rom_art::art_set *art);

// ---------------------------------------------------------------- settings (U3.2)
struct settings_state
{
	int shape = 0;                       // 0 original 4:3, 1 stretch
	int scaling = 1;                     // 0 integer, 1 fit, 2 smooth
	int shader = 3;
	std::vector<std::string> shaders;
	float scanlines = 0.6f;              // 0.2 to 1
	int sides = 0;                       // index into side_names
	std::vector<std::string> side_names; // "Black", "Blurred picture", then one per overlay image
	bool fullscreen = false;
	float volume = 0.8f;
	bool free_play = true, four_players = true, video_clips = true;
	int country = 0;
	bool news = true;
};

enum settings_id { st_shape = 1, st_scaling, st_shader, st_scanlines, st_sides, st_fullscreen, st_volume, st_free_play, st_players, st_country, st_clips, st_news };

screen make_settings(const settings_state &st);
void show_settings(screen &s, const settings_state &st);        // the widgets take the state's values (a key changed one)
void read_settings(const screen &s, settings_state &st);        // the state takes the widgets' values

} // namespace ui
