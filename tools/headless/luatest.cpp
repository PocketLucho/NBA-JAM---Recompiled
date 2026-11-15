// luatest.cpp - tests of the Lua mod engine that need no game data at all.
// A synthetic, all-zero program ROM stands in for the game; the scripts are written to a
// temporary folder by the test itself. Covers: option parsing, the sandbox, the instruction
// budget, the ROM patch API, per-frame hooks and the mods.ini round trip.
// usage: luatest        (exit code 0 = all passed)
#include "tunit.h"
#include "mods.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else printf("ok    %s\n", msg); } while (0)

static void write_file(const fs::path &p, const std::string &s) { std::ofstream(p, std::ios::binary) << s; }

static std::unique_ptr<tunit_machine> make_machine(u16 planted_at_word, const u16 *words, size_t n)
{
	tunit_roms r;
	r.program.assign(0x100000, 0);
	for (size_t i = 0; i < n; i++) { r.program[(planted_at_word + i) * 2] = u8(words[i]); r.program[(planted_at_word + i) * 2 + 1] = u8(words[i] >> 8); }
	r.gfx.assign(0xc00000, 0);
	r.snd_u3.assign(0x20000, 0xff); r.snd_u12.assign(0x80000, 0xff); r.snd_u13.assign(0x80000, 0xff);
	return std::make_unique<tunit_machine>(std::move(r), tunit_protection::nbajamte);
}

static mod_script *find(mod_manager &mm, const std::string &file)
{
	for (auto &s : mm.mods()) if (s.file == file) return &s;
	return nullptr;
}

int main()
{
	fs::path dir = fs::temp_directory_path() / "nbajam_luatest";
	fs::remove_all(dir);
	fs::create_directories(dir);
	std::string d = dir.string(), ini = (dir / "mods.ini").string();

	// ---- scripts ----
	write_file(dir / "opts.lua", R"(
mod = {
  name = "Options test", description = "d", games = { "nbajamte", "nbajamse" },
  options = {
    { id = "minutes", label = "Minutes", values = { 1, 2, 5, 12 }, default = 5, format = "%d min" },
    { id = "mode", values = { 0, 1 }, labels = { "Off", "On" } },
    { id = "empty", values = {} },
    { label = "no id", values = { 1 } },
  },
}
)");
	write_file(dir / "sandbox.lua", R"(
mod = { name = "Sandbox" }
assert(os == nil and io == nil and package == nil and debug == nil, "library leaked")
assert(require == nil and dofile == nil and loadfile == nil and load == nil, "loader leaked")
assert(string.format("%d", 3) == "3" and math.floor(2.5) == 2 and table.concat({ 1, 2 }) == "12", "base libraries missing")
)");
	write_file(dir / "escape.lua", "mod = { name = 'Escape' }\nos.exit(1)\n");
	write_file(dir / "forever.lua", "mod = { name = 'Forever' }\nwhile true do end\n");
	write_file(dir / "syntax.lua", "mod = { name = \n");
	write_file(dir / "noname.lua", "mod = { }\n");
	write_file(dir / "patch.lua", R"(
mod = { name = "Patch", options = { { id = "v", values = { 7, 9 }, default = 9 } } }
local hits
function on_load()
  hits = nba.rom_find({ 0x1234, 0x5678, -1, 0x9abc })
  nba.log("found " .. #hits)
  nba.rom_write16(hits[1], nba.option("v"))
  nba.rom_write32(hits[1] + 16, 0xdeadbeef)
  assert(nba.rom_read16(hits[1]) == 9)
  assert(nba.rom_read32(hits[1] + 16) == 0xdeadbeef)
  assert(not pcall(nba.rom_read16, 0x1000), "out-of-range read accepted")
  assert(not pcall(nba.rom_write16, nba.ROM_BASE + 8, 1), "unaligned write accepted")
end
)");
	write_file(dir / "frames.lua", R"(
mod = { name = "Frames" }
local last = 0
function on_frame(n) last = n; if n == 3 then nba.log("frame3") end end
)");
	write_file(dir / "frame_loop.lua", R"(
mod = { name = "FrameLoop" }
function on_frame(n) if n == 2 then while true do end end end
)");
	write_file(dir / "frame_err.lua", R"(
mod = { name = "FrameErr" }
function on_frame(n) if n == 2 then error("boom") end end
)");
	write_file(dir / "notes.txt", "not a script");

	mod_manager mm;
	mm.scan(d, ini);

	// ---- scanning and metadata ----
	CHECK(mm.mods().size() == 10, "only .lua files are listed");
	mod_script *o = find(mm, "opts.lua");
	CHECK(o && o->error.empty(), "options script loads");
	if (o)
	{
		CHECK(o->name == "Options test" && o->description == "d", "name and description parsed");
		CHECK(o->supported.size() == 2 && o->supports("nbajamte") && !o->supports("nbajam"), "supported games parsed");
		CHECK(o->options.size() == 2, "options with no values or no id are dropped");
		if (o->options.size() == 2)
		{
			CHECK(o->options[0].id == "minutes" && o->options[0].values.size() == 4, "value list parsed");
			CHECK(o->options[0].def == 2 && o->option_index(o->options[0]) == 2, "default value selects index 2 (5 min)");
			CHECK(o->options[0].format == "%d min", "format string kept");
			CHECK(o->options[1].label == "mode", "label falls back to the id");
			CHECK(o->options[1].labels.size() == 2 && o->options[1].value_text(1) == "On", "value labels used");
		}
	}
	CHECK(find(mm, "noname.lua") && find(mm, "noname.lua")->name == "noname", "missing name falls back to the file name");
	CHECK(find(mm, "syntax.lua") && !find(mm, "syntax.lua")->error.empty(), "syntax error is reported, not fatal");

	// ---- sandbox ----
	CHECK(find(mm, "sandbox.lua") && find(mm, "sandbox.lua")->error.empty(), "os, io, package, debug, require, load* are not available");
	CHECK(find(mm, "escape.lua") && !find(mm, "escape.lua")->error.empty(), "os.exit is an error, the program keeps running");

	// ---- instruction budget ----
	mod_script *fv = find(mm, "forever.lua");
	CHECK(fv && fv->error.find("budget") != std::string::npos, "endless loop at load stops at the instruction budget");

	// ---- running against a synthetic ROM ----
	const u16 pattern[4] = { 0x1234, 0x5678, 0x0001, 0x9abc };
	auto m = make_machine(100, pattern, 4);
	for (auto &s : mm.mods()) s.enabled = true;
	mm.start("nbajamte", *m);
	std::vector<std::string> log = mm.take_log();
	auto has = [&](const std::string &t) { for (auto &l : log) if (l.find(t) != std::string::npos) return true; return false; };
	CHECK(has("found 1"), "rom_find with a wildcard word finds the planted pattern once");
	const std::vector<u8> &rom = *m->program_rom();
	CHECK(rom[200] == 9 && rom[201] == 0, "rom_write16 changed the loaded ROM (option value 9)");
	CHECK(rom[202] == 0xef && rom[203] == 0xbe && rom[204] == 0xad && rom[205] == 0xde, "rom_write32 wrote two words");
	CHECK(has("[Patch]") && !has("[Patch] error"), "ROM script ran without error");
	CHECK(fv && !fv->applies_to("nbajamte"), "a script that failed to load is never started");
	// opts.lua supports only nbajamte/nbajamse, so it ran; a game it does not list is skipped
	m = make_machine(100, pattern, 4);
	mm.start("nbajam", *m);
	CHECK(mm.active_count() > 0, "scripts with no game list run on any game");
	bool opts_running_on_jam = false;
	for (auto &s : mm.mods()) if (s.file == "opts.lua" && s.applies_to("nbajam")) opts_running_on_jam = true;
	CHECK(!opts_running_on_jam, "a script is not applied to a game it does not list");
	m = make_machine(100, pattern, 4);
	mm.start("nbajamte", *m);
	mm.take_log();

	// ---- per-frame hooks and their budget ----
	for (int i = 0; i < 4; i++) mm.frame();
	log = mm.take_log();
	CHECK(has("frame3"), "on_frame receives the frame number");
	CHECK(has("[FrameLoop] error, mod stopped"), "endless on_frame loop is stopped by the budget");
	CHECK(has("[FrameErr] error, mod stopped: ") && has("boom"), "error in on_frame stops that mod only");
	int before = mm.active_count();
	mm.frame();
	CHECK(mm.active_count() == before && mm.active_count() >= 1, "other mods keep running after one fails");
	mm.stop();
	CHECK(mm.active_count() == 0, "stop closes every script");

	// ---- interim netplay guard (roadmap 0.3) ----
	write_file(dir / "ram.lua", "mod = { name = 'Ram' }\nfunction on_load() nba.write16(0x01000000, 1) end\n");
	write_file(dir / "ram_alias.lua", "mod = { name = 'RamAlias' }\nlocal w = nba.write8\n");
	mod_manager g;
	g.scan(d, "");
	for (auto &s : g.mods()) s.enabled = true;
	auto bl = g.netplay_blockers("nbajamte");
	auto blocked = [&](const std::string &n) { for (auto &b : bl) if (b == n) return true; return false; };
	CHECK(blocked("Ram") && blocked("RamAlias"), "netplay guard: scripts that write RAM are listed, also through an alias");
	CHECK(blocked("Frames") && blocked("FrameErr"), "netplay guard: scripts with on_frame are listed");
	CHECK(!blocked("Patch") && !blocked("Options test") && !blocked("Sandbox"), "netplay guard: ROM-only scripts are allowed (rom_write* is not a RAM write)");
	for (auto &s : g.mods()) s.enabled = s.file == "patch.lua" || s.file == "opts.lua";
	CHECK(g.netplay_blockers("nbajamte").empty(), "netplay guard: nothing blocks when only ROM scripts are on");
	for (auto &s : g.mods()) s.enabled = s.file == "ram.lua";
	CHECK(g.netplay_blockers("nbajamte").size() == 1, "netplay guard: only enabled scripts count");
	for (auto &s : g.mods()) s.enabled = false;
	CHECK(g.netplay_blockers("nbajamte").empty(), "netplay guard: a disabled script does not block");
	// backstop: with netplay on, a RAM write is a script error and on_frame does not run
	for (auto &s : g.mods()) s.enabled = s.file == "ram.lua" || s.file == "frames.lua";
	m = make_machine(100, pattern, 4);
	g.set_netplay(true);
	g.start("nbajamte", *m);
	for (int i = 0; i < 4; i++) g.frame();
	log = g.take_log();
	CHECK(has("[Ram] error") && has("online"), "netplay backstop: RAM write raises a script error");
	CHECK(!has("frame3"), "netplay backstop: on_frame is not run");
	g.set_netplay(false);
	g.start("nbajamte", *m);
	for (int i = 0; i < 4; i++) g.frame();
	log = g.take_log();
	CHECK(!has("[Ram] error") && has("frame3"), "offline: the same scripts run normally");
	g.stop();

	// ---- mods.ini round trip ----
	mod_script *p = find(mm, "patch.lua");
	for (auto &s : mm.mods()) s.enabled = false;
	if (p) { p->enabled = true; p->all_games = false; p->games = { "nbajamse", "nbahangt" }; p->opt["v"] = 0; }
	if (o) { o->enabled = true; o->opt["minutes"] = 3; o->opt["mode"] = 1; }
	mm.save();
	mod_manager m2;
	m2.scan(d, ini);
	mod_script *p2 = find(m2, "patch.lua"), *o2 = find(m2, "opts.lua");
	CHECK(p2 && p2->enabled && !p2->all_games, "enabled flag and game list restored");
	CHECK(p2 && p2->games.size() == 2 && p2->games[0] == "nbajamse" && p2->games[1] == "nbahangt", "game list order restored");
	CHECK(p2 && p2->option_index(p2->options[0]) == 0, "option value restored (7)");
	CHECK(o2 && o2->option_index(o2->options[0]) == 3 && o2->option_index(o2->options[1]) == 1, "other script's options restored");
	CHECK(find(m2, "escape.lua") && !find(m2, "escape.lua")->enabled, "disabled scripts stay disabled");
	// an unknown option value in the ini is ignored, the default stays
	write_file(ini, "[opts.lua]\nenabled=1\nminutes=99\nmode=1\n");
	mod_manager m3;
	m3.scan(d, ini);
	mod_script *o3 = find(m3, "opts.lua");
	CHECK(o3 && o3->enabled && o3->option_index(o3->options[0]) == 2 && o3->option_index(o3->options[1]) == 1, "invalid option value falls back to the default");

	fs::remove_all(dir);
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
