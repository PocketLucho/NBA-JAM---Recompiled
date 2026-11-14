// mods.cpp - Lua mod support (see mods.h). Scripts run in a sandbox: only the base, table,
// string, math and utf8 libraries are opened (no io, os, package or debug), and every call
// into a script has an instruction budget so a broken loop cannot hang the emulator.
#include "mods.h"
#include "romload.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// UTF-8 paths on every platform (as in romload.cpp)
static std::string u8s(const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
static fs::path P(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }

// load a script through our own file reader, so non-ASCII folders work on Windows too
static int load_script(lua_State *L, const std::string &path, const std::string &name)
{
	std::vector<u8> d;
	if (!read_whole_file(path, d)) { lua_pushstring(L, ("cannot read " + path).c_str()); return LUA_ERRFILE; }
	return luaL_loadbufferx(L, reinterpret_cast<const char *>(d.data()), d.size(), ("@" + name).c_str(), "t");
}

static const u32 kRomBase = 0xff800000;
static const int kLoadBudget = 200000000;   // instructions for on_load / metadata
static const int kFrameBudget = 5000000;    // instructions per on_frame

// ---------------------------------------------------------------------------
// option / script helpers
// ---------------------------------------------------------------------------
std::string mod_option::value_text(int i) const
{
	if (i < 0 || i >= int(values.size())) return "?";
	if (i < int(labels.size()) && !labels[size_t(i)].empty()) return labels[size_t(i)];
	char b[64];
	double v = values[size_t(i)];
	if (!format.empty())
	{
		if (format.find("%d") != std::string::npos || format.find("%i") != std::string::npos) snprintf(b, sizeof b, format.c_str(), int(v));
		else snprintf(b, sizeof b, format.c_str(), v);
	}
	else if (v == static_cast<double>(static_cast<long long>(v))) snprintf(b, sizeof b, "%lld", (long long)v);
	else snprintf(b, sizeof b, "%g", v);
	return b;
}

bool mod_script::supports(const std::string &g) const
{
	return supported.empty() || std::find(supported.begin(), supported.end(), g) != supported.end();
}

bool mod_script::game_checked(const std::string &g) const
{
	return std::find(games.begin(), games.end(), g) != games.end();
}

bool mod_script::applies_to(const std::string &g) const
{
	if (!enabled || !error.empty() || !supports(g)) return false;
	return all_games || game_checked(g);
}

void mod_script::set_game(const std::string &g, bool on)
{
	auto it = std::find(games.begin(), games.end(), g);
	if (on && it == games.end()) games.push_back(g);
	if (!on && it != games.end()) games.erase(it);
}

int mod_script::option_index(const mod_option &o) const
{
	auto it = opt.find(o.id);
	int i = it == opt.end() ? o.def : it->second;
	return std::clamp(i, 0, std::max(0, int(o.values.size()) - 1));
}

// ---------------------------------------------------------------------------
// the `nba` API seen by scripts
// ---------------------------------------------------------------------------
struct mod_api
{
	static mod_manager *mgr(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_mgr");
		auto *m = static_cast<mod_manager *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return m;
	}
	static mod_script *script(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_mod");
		auto *m = static_cast<mod_script *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return m;
	}
	static arcade_machine &mach(lua_State *L)
	{
		mod_manager *m = mgr(L);
		if (!m || !m->m_mach) luaL_error(L, "no game is running");
		return *m->m_mach;
	}
	static std::vector<u8> &rom(lua_State *L)
	{
		std::vector<u8> *r = mach(L).program_rom();
		if (!r || r->empty()) luaL_error(L, "this game has no program ROM access");
		return *r;
	}
	static size_t rom_word(lua_State *L, int arg)
	{
		lua_Integer a = luaL_checkinteger(L, arg);
		u32 addr = u32(a);
		std::vector<u8> &r = rom(L);
		if (addr < kRomBase || (addr & 15) || (size_t(addr - kRomBase) >> 4) >= r.size() / 2)
			luaL_error(L, "ROM address 0x%08x out of range (bit addresses 0xff800000.., word aligned)", addr);
		return size_t(addr - kRomBase) >> 4;
	}
	static u16 rw(std::vector<u8> &r, size_t w) { return u16(r[w * 2] | (r[w * 2 + 1] << 8)); }
	static void ww(std::vector<u8> &r, size_t w, u16 v) { r[w * 2] = u8(v); r[w * 2 + 1] = u8(v >> 8); }

	static int log(lua_State *L)
	{
		int n = lua_gettop(L);
		std::string s;
		for (int i = 1; i <= n; i++) { if (i > 1) s += " "; s += luaL_tolstring(L, i, nullptr); lua_pop(L, 1); }
		if (mod_manager *m = mgr(L)) m->report(*script(L), s);
		return 0;
	}
	static int option(lua_State *L)
	{
		const char *id = luaL_checkstring(L, 1);
		mod_script *s = script(L);
		for (const auto &o : s->options)
			if (o.id == id) { lua_pushnumber(L, o.values.empty() ? 0 : o.values[size_t(s->option_index(o))]); return 1; }
		return luaL_error(L, "unknown option '%s'", id);
	}
	static int frame(lua_State *L) { lua_pushinteger(L, mgr(L)->m_frame); return 1; }
	static int rom_size(lua_State *L) { lua_pushinteger(L, lua_Integer(rom(L).size() / 2)); return 1; }
	static int rom_read16(lua_State *L) { size_t w = rom_word(L, 1); lua_pushinteger(L, rw(rom(L), w)); return 1; }
	static int rom_read32(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		std::vector<u8> &r = rom(L);
		if (w + 1 >= r.size() / 2) return luaL_error(L, "ROM address out of range");
		lua_pushinteger(L, lua_Integer(rw(r, w) | (u32(rw(r, w + 1)) << 16)));
		return 1;
	}
	static int rom_write16(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		ww(rom(L), w, u16(luaL_checkinteger(L, 2)));
		return 0;
	}
	static int rom_write32(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		std::vector<u8> &r = rom(L);
		if (w + 1 >= r.size() / 2) return luaL_error(L, "ROM address out of range");
		u32 v = u32(luaL_checkinteger(L, 2));
		ww(r, w, u16(v));
		ww(r, w + 1, u16(v >> 16));
		return 0;
	}
	// rom_find({0x09e0, 0x0000, 0x0300, -1, ...} [, start_addr [, max]]) -> { addr, ... }
	// -1 / false matches any word
	static int rom_find(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE);
		std::vector<int> pat;
		lua_Integer n = luaL_len(L, 1);
		for (lua_Integer i = 1; i <= n; i++)
		{
			lua_geti(L, 1, i);
			int v = lua_isboolean(L, -1) ? -1 : int(luaL_checkinteger(L, -1));
			pat.push_back(v < 0 ? -1 : (v & 0xffff));
			lua_pop(L, 1);
		}
		std::vector<u8> &r = rom(L);
		size_t words = r.size() / 2;
		size_t start = lua_isnoneornil(L, 2) ? 0 : rom_word(L, 2);
		lua_Integer max = luaL_optinteger(L, 3, 1000);
		lua_newtable(L);
		if (pat.empty()) return 1;
		lua_Integer found = 0;
		for (size_t w = start; w + pat.size() <= words && found < max; w++)
		{
			size_t k = 0;
			while (k < pat.size() && (pat[k] < 0 || rw(r, w + k) == pat[k])) k++;
			if (k == pat.size())
			{
				lua_pushinteger(L, lua_Integer(kRomBase + (u32(w) << 4)));
				lua_rawseti(L, -2, ++found);
			}
		}
		return 1;
	}
	static int read16(lua_State *L) { lua_pushinteger(L, mach(L).peek16(u32(luaL_checkinteger(L, 1)))); return 1; }
	static int write16(lua_State *L) { mach(L).poke16(u32(luaL_checkinteger(L, 1)), u16(luaL_checkinteger(L, 2))); return 0; }
	static int read32(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1));
		arcade_machine &m = mach(L);
		lua_pushinteger(L, lua_Integer(m.peek16(a) | (u32(m.peek16(a + 16)) << 16)));
		return 1;
	}
	static int write32(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1)), v = u32(luaL_checkinteger(L, 2));
		arcade_machine &m = mach(L);
		m.poke16(a, u16(v));
		m.poke16(a + 16, u16(v >> 16));
		return 0;
	}
	static int read8(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1));
		lua_pushinteger(L, (mach(L).peek16(a & ~15u) >> (a & 8)) & 0xff);
		return 1;
	}
	static int write8(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1));
		u16 v = u16(luaL_checkinteger(L, 2) & 0xff);
		arcade_machine &m = mach(L);
		u16 w = m.peek16(a & ~15u);
		w = (a & 8) ? u16((w & 0x00ff) | (v << 8)) : u16((w & 0xff00) | v);
		m.poke16(a & ~15u, w);
		return 0;
	}
};

static void budget_hook(lua_State *L, lua_Debug *)
{
	luaL_error(L, "script took too long (instruction budget exceeded)");
}

static bool call(lua_State *L, int nargs, int budget, std::string &err)
{
	lua_sethook(L, budget_hook, LUA_MASKCOUNT, budget);
	int r = lua_pcall(L, nargs, 0, 0);
	lua_sethook(L, nullptr, 0, 0);
	if (r == LUA_OK) return true;
	err = lua_isstring(L, -1) ? lua_tostring(L, -1) : "error";
	lua_pop(L, 1);
	return false;
}

lua_State *mod_manager::new_state(mod_script *mod, bool with_game)
{
	lua_State *L = luaL_newstate();
	if (!L) return nullptr;
	static const luaL_Reg libs[] = {
		{ LUA_GNAME, luaopen_base }, { LUA_TABLIBNAME, luaopen_table }, { LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 },
	};
	for (const auto &l : libs) { luaL_requiref(L, l.name, l.func, 1); lua_pop(L, 1); }
	// no loading of other files / bytecode from a sandboxed script
	for (const char *g : { "dofile", "loadfile", "load" }) { lua_pushnil(L); lua_setglobal(L, g); }
	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "nba_mgr");
	lua_pushlightuserdata(L, mod);
	lua_setfield(L, LUA_REGISTRYINDEX, "nba_mod");

	static const luaL_Reg api[] = {
		{ "log", mod_api::log }, { "option", mod_api::option }, { "frame", mod_api::frame },
		{ "rom_size", mod_api::rom_size }, { "rom_read16", mod_api::rom_read16 }, { "rom_read32", mod_api::rom_read32 },
		{ "rom_write16", mod_api::rom_write16 }, { "rom_write32", mod_api::rom_write32 }, { "rom_find", mod_api::rom_find },
		{ "read8", mod_api::read8 }, { "write8", mod_api::write8 }, { "read16", mod_api::read16 }, { "write16", mod_api::write16 },
		{ "read32", mod_api::read32 }, { "write32", mod_api::write32 },
		{ nullptr, nullptr },
	};
	luaL_newlib(L, api);
	lua_pushinteger(L, kRomBase);
	lua_setfield(L, -2, "ROM_BASE");
	if (with_game) lua_pushstring(L, m_game.c_str()); else lua_pushnil(L);
	lua_setfield(L, -2, "game");
	lua_setglobal(L, "nba");
	return L;
}

// ---------------------------------------------------------------------------
// scanning: read each script's `mod` table
// ---------------------------------------------------------------------------
static std::string field_str(lua_State *L, int t, const char *k)
{
	lua_getfield(L, t, k);
	std::string s = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
	lua_pop(L, 1);
	return s;
}

bool mod_manager::load_meta(mod_script &m)
{
	lua_State *L = new_state(&m, false);
	if (!L) { m.error = "out of memory"; return false; }
	std::string err;
	if (load_script(L, m.path, m.file) != LUA_OK)
	{
		m.error = lua_tostring(L, -1);
		lua_close(L);
		return false;
	}
	if (!call(L, 0, kLoadBudget, err)) { m.error = err; lua_close(L); return false; }
	lua_getglobal(L, "mod");
	if (lua_istable(L, -1))
	{
		int t = lua_gettop(L);
		m.name = field_str(L, t, "name");
		m.description = field_str(L, t, "description");
		lua_getfield(L, t, "games");
		if (lua_istable(L, -1))
			for (lua_Integer i = 1; i <= luaL_len(L, -1); i++)
			{
				lua_geti(L, -1, i);
				if (lua_isstring(L, -1)) m.supported.push_back(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		lua_pop(L, 1);
		lua_getfield(L, t, "options");
		if (lua_istable(L, -1))
		{
			int ot = lua_gettop(L);
			for (lua_Integer i = 1; i <= luaL_len(L, ot); i++)
			{
				lua_geti(L, ot, i);
				if (lua_istable(L, -1))
				{
					int o = lua_gettop(L);
					mod_option op;
					op.id = field_str(L, o, "id");
					op.label = field_str(L, o, "label");
					op.format = field_str(L, o, "format");
					if (op.label.empty()) op.label = op.id;
					lua_getfield(L, o, "values");
					if (lua_istable(L, -1))
						for (lua_Integer k = 1; k <= luaL_len(L, -1); k++) { lua_geti(L, -1, k); op.values.push_back(lua_tonumber(L, -1)); lua_pop(L, 1); }
					lua_pop(L, 1);
					lua_getfield(L, o, "labels");
					if (lua_istable(L, -1))
						for (lua_Integer k = 1; k <= luaL_len(L, -1); k++) { lua_geti(L, -1, k); op.labels.push_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : ""); lua_pop(L, 1); }
					lua_pop(L, 1);
					lua_getfield(L, o, "default");
					if (lua_isnumber(L, -1))
					{
						double d = lua_tonumber(L, -1);
						for (size_t k = 0; k < op.values.size(); k++) if (op.values[k] == d) op.def = int(k);
					}
					lua_pop(L, 1);
					if (!op.id.empty() && !op.values.empty()) m.options.push_back(op);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	if (m.name.empty()) m.name = u8s(P(m.file).stem());
	lua_close(L);
	return true;
}

void mod_manager::scan(const std::string &dir, const std::string &ini)
{
	stop();
	m_mods.clear();
	m_ini = ini;
	std::error_code ec;
	std::vector<fs::path> files;
	for (auto it = fs::directory_iterator(P(dir), ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		std::string ext = it->path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (it->is_regular_file(ec) && ext == ".lua") files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	for (const auto &p : files)
	{
		mod_script m;
		m.file = u8s(p.filename());
		m.path = u8s(p);
		load_meta(m);
		m_mods.push_back(std::move(m));
	}
	load_settings();
}

// ---------------------------------------------------------------------------
// settings: mods.ini
//   [quarter_length.lua]
//   enabled=1
//   games=all            (or a comma separated list of game ids)
//   minutes=5            (option values, by value)
// ---------------------------------------------------------------------------
static std::string trim(std::string s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void mod_manager::load_settings()
{
	std::ifstream f(P(m_ini));
	if (!f) return;
	std::string line;
	mod_script *cur = nullptr;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		if (line[0] == '[')
		{
			std::string name = trim(line.substr(1, line.find(']') - 1));
			cur = nullptr;
			for (auto &m : m_mods) if (m.file == name) cur = &m;
			continue;
		}
		if (!cur) continue;
		auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		if (k == "enabled") cur->enabled = atoi(v.c_str()) != 0;
		else if (k == "games")
		{
			cur->all_games = v == "all" || v.empty();
			cur->games.clear();
			if (!cur->all_games)
			{
				std::stringstream ss(v);
				std::string g;
				while (std::getline(ss, g, ',')) if (!trim(g).empty()) cur->games.push_back(trim(g));
			}
		}
		else
			for (const auto &o : cur->options)
				if (o.id == k)
				{
					double d = atof(v.c_str());
					for (size_t i = 0; i < o.values.size(); i++) if (o.values[i] == d) cur->opt[k] = int(i);
				}
	}
}

void mod_manager::save() const
{
	if (m_ini.empty()) return;
	std::ofstream g(P(m_ini));
	g << "# NBA JAM Recomp - Lua mods (scripts in mods/). games=all or a list: nbajam,nbajamte,nbajamse,nbahangt,nbamht\n";
	for (const auto &m : m_mods)
	{
		g << "\n[" << m.file << "]\nenabled=" << (m.enabled ? 1 : 0) << "\ngames=";
		if (m.all_games) g << "all";
		else for (size_t i = 0; i < m.games.size(); i++) g << (i ? "," : "") << m.games[i];
		g << "\n";
		for (const auto &o : m.options) g << o.id << "=" << o.values[size_t(m.option_index(o))] << "\n";
	}
}

// ---------------------------------------------------------------------------
// game session
// ---------------------------------------------------------------------------
void mod_manager::report(const mod_script &m, const std::string &s)
{
	std::string line = "[" + m.name + "] " + s;
	fprintf(stderr, "mod: %s\n", line.c_str());
	m_log.push_back(line);
}

std::vector<std::string> mod_manager::take_log()
{
	std::vector<std::string> r;
	r.swap(m_log);
	return r;
}

void mod_manager::start(const std::string &game_id, arcade_machine &mach)
{
	stop();
	m_game = game_id;
	m_mach = &mach;
	m_frame = 0;
	for (auto &m : m_mods)
	{
		if (!m.applies_to(game_id)) continue;
		lua_State *L = new_state(&m, true);
		if (!L) continue;
		std::string err;
		bool ok = load_script(L, m.path, m.file) == LUA_OK;
		if (!ok) { err = lua_tostring(L, -1); }
		else ok = call(L, 0, kLoadBudget, err);
		if (ok)
		{
			lua_getglobal(L, "on_load");
			if (lua_isfunction(L, -1)) ok = call(L, 0, kLoadBudget, err);
			else lua_pop(L, 1);
		}
		if (!ok)
		{
			report(m, "error: " + err);
			lua_close(L);
			continue;
		}
		lua_getglobal(L, "on_frame");
		bool hf = lua_isfunction(L, -1);
		lua_pop(L, 1);
		m_running.push_back({ &m, L, hf });
	}
}

void mod_manager::frame()
{
	m_frame++;
	for (size_t i = 0; i < m_running.size();)
	{
		running &r = m_running[i];
		if (r.has_frame)
		{
			lua_getglobal(r.L, "on_frame");
			lua_pushinteger(r.L, m_frame);
			std::string err;
			if (!call(r.L, 1, kFrameBudget, err))
			{
				report(*r.mod, "error, mod stopped: " + err);
				lua_close(r.L);
				m_running.erase(m_running.begin() + long(i));
				continue;
			}
		}
		i++;
	}
}

void mod_manager::stop()
{
	for (auto &r : m_running) lua_close(r.L);
	m_running.clear();
	m_mach = nullptr;
}
