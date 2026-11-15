// paths.h - where the program keeps the user's files (roadmap R1.4).
//
// Everything the user owns lives under one data folder: nbajam_recomp.ini, mods.ini,
// saves/, screenshots/, controller_profiles/, mods/, replays/, roms/ and data/.
//
//   portable:  a file called portable.txt next to the executable -> the data folder is the
//              executable's folder, exactly as before this change. The .zip / .tar.gz
//              packages ship with it.
//   installed: otherwise the per-user folder the system expects
//              (Windows %APPDATA%\NBAJamRecomp, Linux ~/.local/share/NBAJamRecomp,
//              macOS ~/Library/Application Support/NBAJamRecomp), so the program can live
//              somewhere it cannot write.
//
// The environment variable NBAJAM_DATA_DIR overrides the data folder (tests, odd setups).
// No SDL in here, so the headless tests can use it.
#pragma once
#include <string>
#include <vector>

struct app_paths
{
	std::string exe_dir;      // where the executable is (read-only in an installed copy)
	std::string data_dir;     // where the user's files are
	bool portable = false;
	bool overridden = false;  // data_dir came from NBAJAM_DATA_DIR
};

// user_dir: the per-user folder for this program (SDL_GetPrefPath in the app).
app_paths resolve_paths(const std::string &exe_dir, const std::string &user_dir, const char *env_override);

// Names inside the data folder that belong to the user (files and folders).
const std::vector<std::string> &user_data_items();

// True when an installed (non-portable) copy finds a setup from an older version next to
// the executable and nothing yet in the data folder: the moment to offer moving it.
bool legacy_setup_present(const app_paths &p);

// Move the old setup from exe_dir to data_dir. Each item is renamed, or copied and then
// removed when a rename is not possible (another drive). An item that cannot be copied
// completely stays where it was. Returns the items that could not be moved; `log` gets one
// line per item.
std::vector<std::string> migrate_legacy_setup(const app_paths &p, std::vector<std::string> *log = nullptr);

// Keep the old behaviour: writes portable.txt next to the executable. False if that folder
// cannot be written.
bool make_portable(const app_paths &p);

// Installed copies: create the data folder with roms/ and mods/, and copy the scripts and
// read-me files shipped next to the executable that the user does not have yet. Never
// overwrites a file. Does nothing in a portable copy.
void seed_data_dir(const app_paths &p);
