// headless_w.cpp - run a Wolf-unit (Hangtime) set headless
#include "wunit.h"
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
	// headless_w prog_lo prog_hi gfx.bin dcsdir frames outprefix
	wunit_roms r;
	auto lo = load(argv[1]), hi = load(argv[2]);
	r.program.resize(0x100000);
	for (size_t i = 0; i < 0x80000; i++) { r.program[i * 2] = lo[i]; r.program[i * 2 + 1] = hi[i]; }
	r.gfx = load(argv[3]);
	std::string d = argv[4];
	r.dcs.assign(0x700000, 0xff);
	for (int k = 0; k < 7; k++)
	{
		auto b = load(d + "U" + std::to_string(k + 2) + ".ROM");
		for (size_t j = 0; j < b.size() && j < 0x100000; j++) r.dcs[k * 0x100000 + j] = b[j];
	}
	wunit_machine m(std::move(r), getenv("PIC") ? atoi(getenv("PIC")) : 528);
	dip_settings ds; m.set_dips(ds);
	int frames = atoi(argv[5]);
	std::vector<float> audio; double peak = 0;
	int every = getenv("EVERY") ? atoi(getenv("EVERY")) : 60;
	for (int f = 1; f <= frames; f++)
	{
		cabinet_inputs in{};
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
					if (what == "coin") in.p[0].coin = true;
					if (what == "right") in.p[0].right = true;
					if (what == "pass") in.p[0].pass = true;
					if (what == "turbo") in.p[0].turbo = true;
				}
			}
		}
		m.set_inputs(in);
		m.run_frame();
		m.take_audio(audio);
		for (float v : audio) peak = std::max(peak, double(std::fabs(v)));
		audio.clear();
		if (f % every == 0)
		{
			printf("frame %d pc=%08x %s audio_peak=%.3f\n", f, m.pc(), m.status().c_str(), peak); fflush(stdout);
			char fn[512]; snprintf(fn, sizeof fn, "%s_%05d.ppm", argv[6], f);
			FILE *o = fopen(fn, "wb");
			fprintf(o, "P6\n%d %d\n255\n", m.width(), m.height());
			for (int i = 0; i < m.width() * m.height(); i++) { u32 c = m.frame()[i]; u8 px[3] = { u8(c >> 16), u8(c >> 8), u8(c) }; fwrite(px, 1, 3, o); }
			fclose(o); peak = 0;
		}
	}
}
