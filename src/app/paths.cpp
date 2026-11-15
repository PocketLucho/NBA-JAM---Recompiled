// paths.cpp - see paths.h
#include "paths.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static fs::path P(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
static std::string S(const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }

app_paths resolve_paths(const std::string &exe_dir, const std::string &user_dir, const char *env_override)
{
	app_paths p;
	p.exe_dir = exe_dir.empty() ? "." : exe_dir;
	std::error_code ec;
	if (env_override && *env_override)
	{
		p.data_dir = env_override;
		p.overridden = true;
		return p;
	}
	if (fs::exists(P(p.exe_dir) / "portable.txt", ec) || user_dir.empty())
	{
		// no per-user folder available: behave as a portable copy rather than fail
		p.data_dir = p.exe_dir;
		p.portable = true;
		return p;
	}
	p.data_dir = user_dir;
	return p;
}

const std::vector<std::string> &user_data_items()
{
	static const std::vector<std::string> items = {
		"nbajam_recomp.ini", "mods.ini", "saves", "screenshots", "controller_profiles", "replays", "mods", "roms", "data",
	};
	return items;
}

// Does this folder hold something a user made or brought? The read-me files and scripts
// that ship in the package do not count on their own.
static bool has_user_content(const fs::path &dir)
{
	std::error_code ec;
	if (fs::exists(dir / "nbajam_recomp.ini", ec) || fs::exists(dir / "mods.ini", ec)) return true;
	for (const char *d : { "saves", "screenshots", "controller_profiles", "replays", "data" })
		if (fs::is_directory(dir / d, ec) && !fs::is_empty(dir / d, ec)) return true;
	if (fs::is_directory(dir / "roms", ec))
		for (auto it = fs::directory_iterator(dir / "roms", ec); !ec && it != fs::directory_iterator(); it.increment(ec))
			if (it->path().filename() != "LEEME_README.txt") return true;
	return false;
}

bool legacy_setup_present(const app_paths &p)
{
	if (p.portable || p.overridden) return false;
	std::error_code ec;
	if (fs::equivalent(P(p.exe_dir), P(p.data_dir), ec)) return false;
	return has_user_content(P(p.exe_dir)) && !has_user_content(P(p.data_dir));
}

// copy src over dst without replacing anything that is already there; true if all of src
// now exists under dst
static bool copy_tree(const fs::path &src, const fs::path &dst)
{
	std::error_code ec;
	if (!fs::is_directory(src, ec))
	{
		fs::create_directories(dst.parent_path(), ec);
		if (fs::exists(dst, ec)) return true;
		return fs::copy_file(src, dst, ec) && fs::file_size(dst, ec) == fs::file_size(src, ec);
	}
	fs::create_directories(dst, ec);
	bool ok = true;
	for (auto it = fs::directory_iterator(src, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
		ok = copy_tree(it->path(), dst / it->path().filename()) && ok;
	return ok && !ec;
}

std::vector<std::string> migrate_legacy_setup(const app_paths &p, std::vector<std::string> *log)
{
	std::vector<std::string> failed;
	std::error_code ec;
	fs::path from = P(p.exe_dir), to = P(p.data_dir);
	fs::create_directories(to, ec);
	for (const auto &name : user_data_items())
	{
		fs::path a = from / name, b = to / name;
		if (!fs::exists(a, ec)) continue;
		bool moved = false;
		if (!fs::exists(b, ec))
		{
			fs::rename(a, b, ec);          // instant on the same drive, also for a large roms/
			moved = !ec;
		}
		if (!moved)
		{
			// another drive, or the target already has this folder: copy what is missing,
			// and remove the original only when everything arrived
			if (copy_tree(a, b)) { fs::remove_all(a, ec); moved = !fs::exists(a, ec); if (!moved && log) log->push_back(name + ": copied; the original could not be removed"); moved = true; }
		}
		if (log) log->push_back(name + (moved ? ": moved" : ": NOT moved"));
		if (!moved) failed.push_back(name);
	}
	return failed;
}

bool make_portable(const app_paths &p)
{
	std::ofstream f(P(p.exe_dir) / "portable.txt");
	f << "This file makes NBA JAM Recomp keep its settings, saves, mods and roms in this folder.\n"
	     "Delete it to use the per-user folder instead.\n"
	     "Con este archivo, NBA JAM Recomp guarda ajustes, partidas, mods y roms en esta carpeta.\n"
	     "Borralo para usar la carpeta de usuario.\n";
	return bool(f);
}

void seed_data_dir(const app_paths &p)
{
	if (p.portable) return;
	std::error_code ec;
	fs::path from = P(p.exe_dir), to = P(p.data_dir);
	if (fs::equivalent(from, to, ec)) return;
	fs::create_directories(to / "roms", ec);
	fs::create_directories(to / "mods", ec);
	// shipped scripts and read-me files, only those the user does not have (never overwrite:
	// the user may have edited a script)
	for (const char *d : { "mods", "roms" })
	{
		if (!fs::is_directory(from / d, ec)) continue;
		for (auto it = fs::directory_iterator(from / d, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
		{
			std::string ext = S(it->path().extension());
			if (!it->is_regular_file(ec) || (ext != ".lua" && ext != ".txt")) continue;
			fs::path dst = to / d / it->path().filename();
			if (!fs::exists(dst, ec)) fs::copy_file(it->path(), dst, ec);
		}
	}
}
