// screens.h - the program's screens rebuilt on the menu toolkit (roadmap U3). No SDL and no
// emulator in here: main.cpp fills a plain description of what there is to show, these
// functions turn it into a toolkit screen and back, and the headless tests walk and draw
// the same screens.
//
//   U3.1  the game picker: the games in a list, greyed when they cannot be played; beside
//         it the highlighted game's name in its own lettering, its revision and, when it
//         cannot be played, why; under it the rest of the program.
//         With no game playable at all it says where the ROMs go.
//   U3.3  Controls: who (game, player, profile), the profile's buttons, and the profile
//         itself. A button is bound by pressing it.
//   U3.4  Lua mods: the scripts in a list; beside it the highlighted script's page (on or
//         off, which games, its options).
//   U3.5  Online: host or join, the address, port and input delay; then the lobby with
//         the players and, for the host, the delay and the start.
//   U3.6  About and help: the program, the controls as they are set now, the program's
//         keys, the credits. News: the notes of the latest release.
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

// ---------------------------------------------------------------- Lua mods (U3.4)
struct mod_view
{
	std::string name, description, error;    // error: the script could not be read; nothing on its page can be changed
	bool enabled = false, all_games = true;
	struct option { std::string label; std::vector<std::string> values; int value = 0; };
	std::vector<option> options;
	struct game { std::string title; bool supported = true, on = false; };
	std::vector<game> games;
};

struct mods_state
{
	std::vector<mod_view> mods;
	std::string folder;                  // where the scripts go
};

enum mods_id { md_mod0 = 1, md_enabled = 500, md_apply, md_opt0 = 600, md_game0 = 700 };

screen make_mods(const mods_state &st);
int mods_index(const screen &s, int last);                 // the script the page is about
void update_mods(screen &s, const mods_state &st, int mod);
// Confirm on a script in the list: focus goes to its page, if there is anything to change there
bool mods_open_page(screen &s);

// ---------------------------------------------------------------- Controls (U3.3)
struct controls_state
{
	std::vector<std::string> games;      // "All games" first
	int game = 0, player = 0;
	std::vector<std::string> profiles;   // the choices of the Profile row, as shown
	int profile = 0;
	std::string pad;                     // the player's controller, empty: none connected
	struct action { std::string name, binding; bool held = false; };
	std::vector<action> actions;
	int capturing = -1, seconds = 0;     // the action waiting for a press, and how long it still waits
	std::vector<std::string> shortcut_sets;
	int shortcut[2] = { 0, 0 };          // what Shortcut 1 and 2 press: index into shortcut_sets
	int deadzone = 35;                   // percent
	bool naming = false;                 // a new profile's name is being typed
	std::string name;
};

enum controls_id { ct_game = 1, ct_player, ct_profile, ct_act0 = 100, ct_short0 = 200, ct_dead = 210, ct_new, ct_reset, ct_delete, ct_export, ct_export_setup, ct_import };

screen make_controls(const controls_state &st);
void update_controls(screen &s, const controls_state &st);

// ---------------------------------------------------------------- Online (U3.5)
struct online_state
{
	std::string game;                    // the highlighted game: the one a host starts
	bool available = true;               // its ROMs are there, so it can be hosted
	std::string address;                 // of the host to join
	int port = 7845, delay = 4;
	std::vector<std::string> local_ips;
	int typing = 0;                      // 1: the address is being typed, 2: the port
	std::string typed;
};

enum online_id { on_host = 1, on_join, on_address, on_port, on_delay };

screen make_online(const online_state &st);
void update_online(screen &s, const online_state &st);

// The lobby: the host sees who has joined, sets the input delay and starts; a player who
// joined sees how the connection stands and waits.
struct lobby_state
{
	bool host = true;
	std::string game;
	std::vector<std::string> local_ips;
	int port = 7845, delay = 4;
	struct player { std::string name, status; };
	std::vector<player> players;         // host: one per player, the host first
	std::string address;                 // joining: the host's address
	int slot = -1, ping = 0, count = 0;  // joining: slot -1 until the host answers
	bool receiving = false;              // joining: the game data is coming
};

enum lobby_id { lb_start = 1, lb_delay, lb_leave };

screen make_lobby(const lobby_state &st);
void update_lobby(screen &s, const lobby_state &st);

// ---------------------------------------------------------------- About and help (U3.6)
struct about_state
{
	std::string version, build, data_dir;
	bool portable = false;
	std::string latest;                  // the latest release's tag; empty: not known
	bool latest_new = false;
	// the Controls tab: what each action is on now, for one player, from the keys in the
	// settings file and from the player's controller profile
	int player = 0;
	std::string game, profile, pad;      // pad: the player's controller, empty: none connected
	struct row { std::string action, key, binding; };
	std::vector<row> controls;           // key is empty for actions the keyboard does not have
};

enum about_id { ab_player = 1 };

screen make_about(const about_state &st);
void update_about(screen &s, const about_state &st);

// ---------------------------------------------------------------- News
struct news_state
{
	bool on = true, ok = false, is_new = false;
	std::string name, date, version, page;
	std::vector<std::string> lines;      // the release notes as plain lines; a line all in capitals is a heading
};

screen make_news(const news_state &st);

} // namespace ui
