// games.h - the NBA JAM saga catalogue and machine construction
#pragma once
#include "machine.h"
#include "romload.h"
#include "rom_art.h"
#include <memory>
#include <string>
#include <vector>

struct game_info
{
	const char *id;          // short id (nvram / config names)
	const char *title;
	const char *subtitle;
	const char *hardware;    // "T-Unit" / "Wolf Unit"
	bool from_source;        // our build from the original source code is identical to this ROM
};

const std::vector<game_info> &game_list();

// Games are loaded from the user's own ROM files (MAME romsets or the ZIPs of the public
// source repositories, put in roms/ and found by CRC32); if they are not there, from
// data/<id>/ when the user has built the game from source with tools/build_roms.sh.
// No game data ships with the program.
class game_catalog
{
public:
	explicit game_catalog(const std::string &base_dir);
	// availability check (fast) and a human-readable reason when missing (in the UI language)
	bool available(const game_info &g, std::string &why);
	std::unique_ptr<arcade_machine> create(const game_info &g, std::string &err);
	// program and graphics ROMs only, for the menus' art reader (rom_art.h); no sound needed
	bool art_roms(const game_info &g, rom_art::roms &out);
	std::string data_dir(const game_info &g) const;
	bool uses_data_dir(const game_info &g) const;        // data/<id>/ holds an own build
	void rescan() { m_pool = rom_pool(); m_pool_scanned = false; }
	std::string base() const { return m_base; }
private:
	std::string m_base;
	rom_pool m_pool;
	bool m_pool_scanned = false;
	rom_pool &pool();
	bool from_roms(const game_info &g, std::string &why);   // everything found in roms/
};
