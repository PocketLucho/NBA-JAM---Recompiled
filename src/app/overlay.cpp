// overlay.cpp - see overlay.h
#include "overlay.h"
#include "png_read.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

namespace fs = std::filesystem;

static fs::path P(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
static std::string S(const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
static std::string lower(std::string s) { for (char &c : s) c = char(tolower(static_cast<unsigned char>(c))); return s; }
static std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string overlay_image_from_cfg(const std::string &cfg_path)
{
	std::ifstream f(P(cfg_path));
	std::string line, first, zero;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#') continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = lower(trim(line.substr(0, eq))), v = trim(line.substr(eq + 1));
		// overlay0_overlay = "border.png"; other overlayN_overlay only if there is no overlay0
		if (k.size() < 16 || k.compare(0, 7, "overlay") != 0 || k.compare(k.size() - 8, 8, "_overlay") != 0) continue;
		if (v.size() >= 2 && v.front() == '"') v = v.substr(1, v.find('"', 1) == std::string::npos ? std::string::npos : v.find('"', 1) - 1);
		else if (v.find('#') != std::string::npos) v = trim(v.substr(0, v.find('#')));
		if (v.empty()) continue;
		if (k == "overlay0_overlay") zero = v;
		else if (first.empty()) first = v;
	}
	std::string img = zero.empty() ? first : zero;
	if (img.empty()) return "";
	std::replace(img.begin(), img.end(), '\\', '/');
	fs::path p = P(img);
	// only files next to (or below) the .cfg: an overlay file has no business pointing elsewhere
	if (p.is_absolute() || img.find("..") != std::string::npos) return "";
	return S(P(cfg_path).parent_path() / p);
}

std::vector<overlay_entry> list_overlays(const std::string &data_dir)
{
	std::vector<overlay_entry> out;
	std::error_code ec;
	fs::path dir = P(data_dir) / "overlays";
	std::vector<fs::path> pngs, cfgs;
	for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		if (!it->is_regular_file(ec)) continue;
		std::string ext = lower(S(it->path().extension()));
		if (ext == ".png") pngs.push_back(it->path());
		else if (ext == ".cfg") cfgs.push_back(it->path());
	}
	std::set<std::string> used;      // images that a .cfg already stands for
	for (const auto &c : cfgs)
	{
		std::string img = overlay_image_from_cfg(S(c));
		if (img.empty() || !fs::is_regular_file(P(img), ec)) continue;
		out.push_back({ S(c.stem()), img });
		used.insert(lower(S(fs::weakly_canonical(P(img), ec))));
	}
	for (const auto &p : pngs)
		if (!used.count(lower(S(fs::weakly_canonical(p, ec))))) out.push_back({ S(p.stem()), S(p) });
	fs::path old = P(data_dir) / "bezel.png";
	if (fs::is_regular_file(old, ec)) out.push_back({ "bezel", S(old) });
	std::sort(out.begin(), out.end(), [](const overlay_entry &a, const overlay_entry &b) { return lower(a.name) < lower(b.name); });
	return out;
}

bool load_overlay(const overlay_entry &e, std::vector<uint8_t> &rgba, int &w, int &h, std::string &err)
{
	std::ifstream f(P(e.image), std::ios::binary);
	if (!f) { err = "cannot open " + e.image; return false; }
	std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return png_decode(file, rgba, w, h, err);
}
