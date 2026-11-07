// cattest.cpp - run a catalogue game headless with a scripted input pattern
#include "games.h"
#include <cstdio>
int main(int argc, char **argv)
{
	std::string base = argv[1], id = argv[2];
	int frames = atoi(argv[3]);
	const char *out = argv[4];
	game_catalog cat(base);
	for (auto &g : game_list())
	{
		if (id != g.id) continue;
		std::string why, err;
		printf("available=%d %s\n", cat.available(g, why), why.c_str());
		auto m = cat.create(g, err);
		if (!m) { printf("create failed: %s\n", err.c_str()); return 1; }
		dip_settings d; m->set_dips(d); m->reset();
		static offs_t tr[4096];
		if (getenv("TRACE") && m->maincpu()) { m->maincpu()->m_trace = tr; m->maincpu()->m_trace_lo = 0xff800000; }
		bool dumped = false;
		static offs_t str[4096];
		if (getenv("STRACE") && m->soundcpu()) { m->soundcpu()->m_trace = str; if (getenv("SFIRST")) m->soundcpu()->m_trace_max = atoi(getenv("SFIRST")); if (getenv("SSKIP2")) { m->soundcpu()->m_skip2_lo = strtol(getenv("SSKIP2"),0,16); m->soundcpu()->m_skip2_hi = strtol(strchr(getenv("SSKIP2"),'-')+1,0,16); } if (getenv("SSKIP")) { m->soundcpu()->m_skip_lo = strtol(getenv("SSKIP"),0,16); m->soundcpu()->m_skip_hi = strtol(strchr(getenv("SSKIP"),'-')+1,0,16); } }
		int stf = getenv("STRACE") ? atoi(getenv("STRACE")) : -1;
		std::vector<float> a; double peak = 0;
		int every = getenv("EVERY") ? atoi(getenv("EVERY")) : 120;
		for (int f = 1; f <= frames; f++)
		{
			cabinet_inputs in{};
			if (f >= 600 && f < 606) in.p[0].shoot = true;            // dismiss error screen
			if (f >= 900 && f < 906) in.p[0].start = true;            // join
			if (f > 960 && (f % 50) < 4) in.p[0].shoot = true;        // confirm everything
			if (f > 1400 && (f % 200) < 60) in.p[0].right = true;     // move around in game
			if (f > 1400 && (f % 90) < 5) in.p[0].turbo = true;
			m->set_inputs(in);
			m->run_frame();
			if (f == stf) { auto *c = m->soundcpu(); printf("SND trace pos=%u:", c->m_trace_pos); if (getenv("SFIRST")) { for (u32 i = 0; i < c->m_trace_pos && i < 4096; i++) printf(" %x", str[i]); } else for (int i = 300; i > 0; i--) printf(" %x", str[(c->m_trace_pos - i) & 4095]); printf("\n"); }
			if (m->maincpu() && m->maincpu()->m_trace && !dumped && m->pc() < 0xff800000)
			{
				dumped = true; auto *c = m->maincpu();
				printf("CRASH at frame %d, last PCs:\n", f);
				for (int i = 4000; i > 0; i--) { offs_t p = tr[(c->m_trace_pos - i) & 4095]; if (i < 4000 && p != tr[(c->m_trace_pos - i - 1) & 4095]) printf(" %08x", p); }
				printf("\n");
			}
			m->take_audio(a);
			{ static FILE *wf = getenv("WAVOUT") ? fopen(getenv("WAVOUT"), "wb") : nullptr; if (wf) { for (float v : a) { short x = short(std::max(-1.f, std::min(1.f, v)) * 32767); fwrite(&x, 2, 1, wf); } fflush(wf); } }
			for (float v : a) peak = std::max(peak, double(std::fabs(v)));
			a.clear();
			if (f % every == 0)
			{
				printf("frame %d pc=%08x %s peak=%.3f\n", f, m->pc(), m->status().c_str(), peak); fflush(stdout); peak = 0;
				char fn[512]; snprintf(fn, sizeof fn, "%s_%05d.ppm", out, f);
				FILE *o = fopen(fn, "wb");
				fprintf(o, "P6\n%d %d\n255\n", m->width(), m->height());
				for (int i = 0; i < m->width() * m->height(); i++) { u32 c = m->frame()[i]; u8 px[3] = { u8(c >> 16), u8(c >> 8), u8(c) }; fwrite(px, 1, 3, o); }
				fclose(o);
			}
		}
	}
}
