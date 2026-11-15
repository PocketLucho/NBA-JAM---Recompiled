// pathtest.cpp - tests of the user data folder layer (src/app/paths.cpp). No ROM, no SDL.
// usage: pathtest        (exit code 0 = all passed)
#include "paths.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else printf("ok    %s\n", msg); } while (0)

static void put(const fs::path &p, const std::string &s) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << s; }
static std::string get(const fs::path &p) { std::ifstream f(p, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }

int main()
{
	fs::path root = fs::temp_directory_path() / "nbajam_pathtest";
	fs::remove_all(root);
	fs::path exe = root / "program", user = root / "user";
	fs::create_directories(exe);

	// ---- which folder ----
	app_paths p = resolve_paths(exe.string(), user.string(), nullptr);
	CHECK(!p.portable && p.data_dir == user.string(), "no portable.txt: data goes to the per-user folder");
	put(exe / "portable.txt", "x");
	p = resolve_paths(exe.string(), user.string(), nullptr);
	CHECK(p.portable && p.data_dir == exe.string(), "portable.txt: data stays next to the executable");
	p = resolve_paths(exe.string(), user.string(), (root / "elsewhere").string().c_str());
	CHECK(p.overridden && !p.portable && p.data_dir == (root / "elsewhere").string(), "NBAJAM_DATA_DIR wins over both");
	fs::remove(exe / "portable.txt");
	p = resolve_paths(exe.string(), "", nullptr);
	CHECK(p.portable && p.data_dir == exe.string(), "no per-user folder available: falls back to portable");

	// ---- a fresh installed copy: package files only ----
	put(exe / "mods" / "quarter_length.lua", "shipped v2");
	put(exe / "mods" / "README.txt", "readme");
	put(exe / "roms" / "README.txt", "put roms here");
	put(exe / "README.txt", "docs");
	p = resolve_paths(exe.string(), user.string(), nullptr);
	CHECK(!legacy_setup_present(p), "a fresh package (scripts and read-me only) is not an old setup");
	seed_data_dir(p);
	CHECK(get(user / "mods" / "quarter_length.lua") == "shipped v2" && fs::exists(user / "roms" / "README.txt"), "first run copies the shipped scripts and creates roms/");
	put(user / "mods" / "quarter_length.lua", "edited by the user");
	put(exe / "mods" / "new_mod.lua", "new in this version");
	seed_data_dir(p);
	CHECK(get(user / "mods" / "quarter_length.lua") == "edited by the user", "a script the user edited is never overwritten");
	CHECK(fs::exists(user / "mods" / "new_mod.lua"), "a script added by a new version is copied");
	CHECK(!fs::exists(user / "README.txt"), "program documentation is not copied");
	fs::remove_all(user);

	// ---- an old setup next to the executable ----
	put(exe / "nbajam_recomp.ini", "language=es");
	put(exe / "mods.ini", "[quarter_length.lua]");
	put(exe / "saves" / "nbajamte.nv", "nvram");
	put(exe / "screenshots" / "a.png", "png");
	put(exe / "controller_profiles" / "Default.ini", "pad");
	put(exe / "roms" / "nbajam.bin", std::string(5000, 'r'));
	put(exe / "roms" / "sub" / "x.bin", "deep");
	put(exe / "mods" / "mine.lua", "my own mod");
	CHECK(legacy_setup_present(p), "settings, saves and roms next to the executable are an old setup");
	put(exe / "portable.txt", "x");
	CHECK(!legacy_setup_present(resolve_paths(exe.string(), user.string(), nullptr)), "a portable copy is never asked to move");
	fs::remove(exe / "portable.txt");

	std::vector<std::string> log;
	auto failed = migrate_legacy_setup(p, &log);
	CHECK(failed.empty(), "every item moved");
	CHECK(get(user / "nbajam_recomp.ini") == "language=es" && get(user / "saves" / "nbajamte.nv") == "nvram", "settings and saves arrived");
	CHECK(get(user / "roms" / "nbajam.bin").size() == 5000 && get(user / "roms" / "sub" / "x.bin") == "deep", "roms arrived, including sub-folders");
	CHECK(get(user / "mods" / "mine.lua") == "my own mod" && get(user / "controller_profiles" / "Default.ini") == "pad", "the user's mods and controller profiles arrived");
	CHECK(!fs::exists(exe / "nbajam_recomp.ini") && !fs::exists(exe / "saves") && !fs::exists(exe / "roms"), "nothing of the user's is left next to the executable");
	CHECK(fs::exists(exe / "README.txt"), "program files are not touched");
	CHECK(!legacy_setup_present(p), "the offer is not made twice");

	// ---- moving into a data folder that already has some of it (copy path, no overwrite) ----
	fs::remove_all(root);
	put(exe / "nbajam_recomp.ini", "old");
	put(exe / "saves" / "a.nv", "old a");
	put(exe / "saves" / "b.nv", "old b");
	put(user / "saves" / "a.nv", "newer a");
	p = resolve_paths(exe.string(), user.string(), nullptr);
	failed = migrate_legacy_setup(p);
	CHECK(failed.empty() && get(user / "saves" / "a.nv") == "newer a" && get(user / "saves" / "b.nv") == "old b", "merge: missing files are added, existing ones kept");
	CHECK(!fs::exists(exe / "saves"), "merge: the original folder is removed once everything is there");

	// ---- keeping the old behaviour ----
	fs::remove_all(root);
	put(exe / "nbajam_recomp.ini", "old");
	p = resolve_paths(exe.string(), user.string(), nullptr);
	fs::create_directories(user);      // as SDL_GetPrefPath leaves it
	CHECK(make_portable(p), "portable.txt can be written");
	CHECK(!fs::exists(user), "choosing portable removes the empty per-user folder");
	p = resolve_paths(exe.string(), user.string(), nullptr);
	CHECK(p.portable && p.data_dir == exe.string() && get(exe / "nbajam_recomp.ini") == "old", "after choosing portable, the old setup is used as it is");
	seed_data_dir(p);
	CHECK(!fs::exists(user), "a portable copy writes nothing to the per-user folder");

	fs::remove_all(root);
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
