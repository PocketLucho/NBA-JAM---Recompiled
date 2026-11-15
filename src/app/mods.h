// mods.h - Lua mod support. Scripts live in <app>/mods/*.lua; each one can be turned on
// for all games or for a chosen set of games, and may expose options for the mod menu.
// Settings are kept in <app>/mods.ini.
//
// A script declares itself with a global table and two optional hooks:
//
//   mod = {
//     name = "Quarter length", description = "...",
//     games = { "nbajamte", "nbajamse" },       -- optional: games the mod supports
//     options = { { id = "minutes", label = "Minutes per quarter",
//                   values = { 1, 2, 3, 4, 5 }, default = 3, format = "%d min" } },
//   }
//   function on_load() ... end     -- game loaded, before the first reset (patch ROM here)
//   function on_frame(n) ... end   -- after every emulated frame
//
// The `nba` table gives scripts access to the running game (see mods.cpp / mods/README.txt).
#pragma once
#include "machine.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

struct mod_option
{
	std::string id, label, format;
	std::vector<double> values;
	std::vector<std::string> labels;     // optional text for each value
	int def = 0;                         // index into values
	std::string value_text(int i) const;
};

struct mod_script
{
	std::string file;                    // file name inside mods/ (the settings key)
	std::string path;
	std::string name, description, error;
	std::vector<std::string> supported;  // empty = any game
	std::vector<mod_option> options;
	// user settings
	bool enabled = false;
	bool all_games = true;
	std::vector<std::string> games;      // used when !all_games
	std::map<std::string, int> opt;      // option id -> index into values

	bool supports(const std::string &game) const;
	bool applies_to(const std::string &game) const;
	bool game_checked(const std::string &game) const;
	void set_game(const std::string &game, bool on);
	int option_index(const mod_option &o) const;
};

class mod_manager
{
public:
	void scan(const std::string &mods_dir, const std::string &ini_path);
	void save() const;
	std::vector<mod_script> &mods() { return m_mods; }

	// game session
	void start(const std::string &game_id, arcade_machine &m);  // runs on_load for every applicable mod
	void frame();                                                // runs on_frame
	void stop();
	std::vector<std::string> take_log();                         // messages for the UI
	int active_count() const { return int(m_running.size()); }

private:
	struct running
	{
		mod_script *mod;
		lua_State *L;
		bool has_frame;
	};
	std::vector<mod_script> m_mods;
	std::vector<running> m_running;
	std::string m_ini;
	std::string m_game;
	arcade_machine *m_mach = nullptr;
	u32 m_frame = 0;
	std::vector<std::string> m_log;

	lua_State *new_state(mod_script *mod, bool with_game);
	bool load_meta(mod_script &m);
	void load_settings();
	void report(const mod_script &m, const std::string &s);
	friend struct mod_api;
};
