// headless.cpp - run a T-unit romset without a window, dump frames as PPM
#include "tunit.h"
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
	if (argc < 7) { fprintf(stderr, "usage: headless prog_lo prog_hi gfxdir_prefix snddir frames outprefix [te|jam]\n"); return 1; }
	tunit_roms r;
	auto lo = load(argv[1]), hi = load(argv[2]);
	r.program.resize(0x100000);
	for (size_t i = 0; i < 0x80000; i++) { r.program[i * 2] = lo[i]; r.program[i * 2 + 1] = hi[i]; }
	std::string g = argv[3];
	const char *lanes[4][4] = { {"ug14","uj14","ug19","uj19"},{"ug16","uj16","ug20","uj20"},{"ug17","uj17","ug22","uj22"},{"ug18","uj18","ug23","uj23"} };
	r.gfx.assign(0xc00000, 0);
	if (g.size() > 4 && g.substr(g.size() - 4) == ".bin")
	{
		auto d = load(g);
		std::copy(d.begin(), d.begin() + std::min(d.size(), r.gfx.size()), r.gfx.begin());
	}
	else
	for (int b = 0; b < 4; b++)
		for (int l = 0; l < 4; l++)
		{
			auto d = load(g + lanes[b][l] + "." + lanes[b][l]);
			for (size_t k = 0; k < 0x80000; k++) r.gfx[b * 0x200000 + k * 4 + l] = d[k];
		}
	std::string s = argv[4];
	r.snd_u3 = load(s + "l1_nba_jam_tournament_u3_sound_rom.u3");
	r.snd_u12 = load(s + "l1_nba_jam_tournament_u12_sound_rom.u12");
	r.snd_u13 = load(s + "l1_nba_jam_tournament_u13_sound_rom.u13");
	bool jam = argc > 7 && std::string(argv[7]) == "jam";
	tunit_machine m(std::move(r), jam ? tunit_protection::nbajam : tunit_protection::nbajamte);
	dip_settings d; m.set_dips(d);
	int frames = atoi(argv[5]);
	std::vector<float> audio;
	double peak = 0;
	for (int f = 1; f <= frames; f++)
	{
		cabinet_inputs in{};
		// scripted presses: env PRESS="frame:what,frame:what" what=shoot|start|coin|test
		if (const char *pr = getenv("PRESS"))
		{
			std::string ps = pr; size_t pos = 0;
			while (pos < ps.size())
			{
				size_t c = ps.find(',', pos); if (c == std::string::npos) c = ps.size();
				std::string item = ps.substr(pos, c - pos); pos = c + 1;
				int fr = atoi(item.c_str()); std::string what = item.substr(item.find(':') + 1);
				if (f >= fr && f < fr + 6)
				{
					if (what == "shoot") in.p[0].shoot = true;
					if (what == "start") in.p[0].start = true;
					if (what == "start2") in.p[1].start = true;
					if (what == "coin") in.p[0].coin = true;
					if (what == "right") in.p[0].right = true;
					if (what == "pass") in.p[0].pass = true;
				}
			}
		}
		m.set_inputs(in);
		m.run_frame();
		m.take_audio(audio);
		for (float v : audio) peak = std::max(peak, double(std::fabs(v)));
		audio.clear();
		int every = getenv("EVERY") ? atoi(getenv("EVERY")) : 60;
		if (f % every == 0)
		{
			printf("frame %d pc=%08x %s audio_peak=%.3f\n", f, m.pc(), m.status().c_str(), peak);
			fflush(stdout);
			char fn[512]; snprintf(fn, sizeof fn, "%s_%05d.ppm", argv[6], f);
			FILE *o = fopen(fn, "wb");
			fprintf(o, "P6\n%d %d\n255\n", m.width(), m.height());
			for (int i = 0; i < m.width() * m.height(); i++) { u32 c = m.frame()[i]; u8 px[3] = { u8(c >> 16), u8(c >> 8), u8(c) }; fwrite(px, 1, 3, o); }
			fclose(o);
			peak = 0;
		}
	}
	return 0;
}
