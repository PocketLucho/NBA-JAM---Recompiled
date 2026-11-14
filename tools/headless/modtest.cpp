// modtest.cpp - load a T-unit program ROM, run the Lua mods in a folder on it and show
// what they changed. usage: modtest prog_lo prog_hi mods_dir game_id [frames] [opt=value...]
#include "tunit.h"
#include "wunit.h"
#include <memory>
#include "mods.h"
#include <cstdio>
#include <fstream>
#include <iterator>

static std::vector<u8> load(const std::string &p)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) { fprintf(stderr, "missing %s\n", p.c_str()); exit(1); }
	return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: modtest prog_lo prog_hi mods_dir game_id [frames] [opt=value...]\n"); return 1; }
	auto lo = load(argv[1]), hi = load(argv[2]);
	std::vector<u8> prog(0x100000);
	for (size_t i = 0; i < 0x80000; i++) { prog[i * 2] = lo[i]; prog[i * 2 + 1] = hi[i]; }
	std::vector<u8> orig = prog;
	// Wolf-unit games (nbahangt / nbamht) get a Wolf-unit board; PIC=<n> as in headless_w
	std::string gid = argv[4];
	std::unique_ptr<arcade_machine> mp;
	if (gid == "nbahangt" || gid == "nbamht")
	{
		wunit_roms r;
		r.program = prog;
		r.gfx.assign(0x800000, 0);
		r.dcs.assign(0x700000, 0xff);
		mp = std::make_unique<wunit_machine>(std::move(r), getenv("PIC") ? atoi(getenv("PIC")) : 528);
	}
	else
	{
		tunit_roms r;
		r.program = prog;
		r.gfx.assign(0xc00000, 0);
		r.snd_u3.assign(0x20000, 0xff); r.snd_u12.assign(0x80000, 0xff); r.snd_u13.assign(0x80000, 0xff);
		mp = std::make_unique<tunit_machine>(std::move(r), tunit_protection::nbajamte);
	}
	arcade_machine &m = *mp;
	mod_manager mods;
	mods.scan(argv[3], "");
	int frames = argc > 5 ? atoi(argv[5]) : 0;
	for (auto &s : mods.mods())
	{
		s.enabled = true;
		printf("mod %s: \"%s\" %s\n", s.file.c_str(), s.name.c_str(), s.error.empty() ? "" : ("ERROR " + s.error).c_str());
		for (int a = 6; a < argc; a++)
		{
			std::string kv = argv[a]; auto eq = kv.find('=');
			for (auto &o : s.options)
				if (o.id == kv.substr(0, eq))
					for (size_t i = 0; i < o.values.size(); i++) if (o.values[i] == atof(kv.c_str() + eq + 1)) s.opt[o.id] = int(i);
		}
	}
	mods.start(argv[4], m);
	const std::vector<u8> &now = *m.program_rom();
	for (size_t w = 0; w < now.size() / 2; w++)
		if (now[w * 2] != orig[w * 2] || now[w * 2 + 1] != orig[w * 2 + 1])
			printf("  patched %08X: %04X -> %04X\n", 0xff800000u + u32(w << 4), orig[w * 2] | orig[w * 2 + 1] << 8, now[w * 2] | now[w * 2 + 1] << 8);
	m.set_dips(dip_settings{});
	m.reset();
	// optional: WATCH=<bit address> prints that long every 500 frames; PLAY=1 presses
	// start then shoot/pass periodically to get through the menus into a game
	const char *watch = getenv("WATCH");
	bool play = getenv("PLAY") != nullptr;
	int p0 = getenv("PLAYAT") ? atoi(getenv("PLAYAT")) : 400;   // first frame of the presses
	for (int f = 0; f < frames; f++)
	{
		cabinet_inputs in{};
		if (play && f > p0)
		{
			int t = f % 60;
			in.p[0].start = f < p0 + 800 && t < 4;
			in.p[0].shoot = getenv("NOPICK") == nullptr && f >= p0 + 800 && t >= 20 && t < 24;
		}
		m.set_inputs(in);
		m.run_frame();
		mods.frame();
		if (watch && f % 500 == 0)
		{
			u32 a = u32(strtoul(watch, nullptr, 0));
			printf("  frame %6d  [%08X] = %08X  pc=%08X\n", f, a, m.peek16(a) | (u32(m.peek16(a + 16)) << 16), m.pc());
		}
	}
	if (frames) printf("ran %d frames, pc=%08X\n", frames, m.pc());
	mods.stop();
	return 0;
}
