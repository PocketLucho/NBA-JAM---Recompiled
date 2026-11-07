// tunit.cpp - Midway T-unit board (see header)
#include "tunit.h"

static const u32 nbajam_prot_values[128] =
{
	0x21283b3b, 0x2439383b, 0x31283b3b, 0x302b3938, 0x31283b3b, 0x302b3938, 0x232f2f2f, 0x26383b3b,
	0x21283b3b, 0x2439383b, 0x312a1224, 0x302b1120, 0x312a1224, 0x302b1120, 0x232d283b, 0x26383b3b,
	0x2b3b3b3b, 0x2e2e2e2e, 0x39383b1b, 0x383b3b1b, 0x3b3b3b1b, 0x3a3a3a1a, 0x2b3b3b3b, 0x2e2e2e2e,
	0x2b39383b, 0x2e2e2e2e, 0x393a1a18, 0x383b1b1b, 0x3b3b1b1b, 0x3a3a1a18, 0x2b39383b, 0x2e2e2e2e,
	0x01202b3b, 0x0431283b, 0x11202b3b, 0x1021283b, 0x11202b3b, 0x1021283b, 0x03273b3b, 0x06302b39,
	0x09302b39, 0x0c232f2f, 0x19322e06, 0x18312a12, 0x19322e06, 0x18312a12, 0x0b31283b, 0x0e26383b,
	0x03273b3b, 0x06302b39, 0x11202b3b, 0x1021283b, 0x13273938, 0x12243938, 0x03273b3b, 0x06302b39,
	0x0b31283b, 0x0e26383b, 0x19322e06, 0x18312a12, 0x1b332f05, 0x1a302b11, 0x0b31283b, 0x0e26383b,
	0x21283b3b, 0x2439383b, 0x31283b3b, 0x302b3938, 0x31283b3b, 0x302b3938, 0x232f2f2f, 0x26383b3b,
	0x21283b3b, 0x2439383b, 0x312a1224, 0x302b1120, 0x312a1224, 0x302b1120, 0x232d283b, 0x26383b3b,
	0x2b3b3b3b, 0x2e2e2e2e, 0x39383b1b, 0x383b3b1b, 0x3b3b3b1b, 0x3a3a3a1a, 0x2b3b3b3b, 0x2e2e2e2e,
	0x2b39383b, 0x2e2e2e2e, 0x393a1a18, 0x383b1b1b, 0x3b3b1b1b, 0x3a3a1a18, 0x2b39383b, 0x2e2e2e2e,
	0x01202b3b, 0x0431283b, 0x11202b3b, 0x1021283b, 0x11202b3b, 0x1021283b, 0x03273b3b, 0x06302b39,
	0x09302b39, 0x0c232f2f, 0x19322e06, 0x18312a12, 0x19322e06, 0x18312a12, 0x0b31283b, 0x0e26383b,
	0x03273b3b, 0x06302b39, 0x11202b3b, 0x1021283b, 0x13273938, 0x12243938, 0x03273b3b, 0x06302b39,
	0x0b31283b, 0x0e26383b, 0x19322e06, 0x18312a12, 0x1b332f05, 0x1a302b11, 0x0b31283b, 0x0e26383b
};

static const u32 nbajamte_prot_values[128] =
{
	0x00000000, 0x04081020, 0x08102000, 0x0c183122, 0x10200000, 0x14281020, 0x18312204, 0x1c393326,
	0x20000001, 0x24081021, 0x28102000, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00000102, 0x04081122, 0x08102102, 0x0c183122, 0x10200000, 0x14281020, 0x18312204, 0x1c393326,
	0x20000103, 0x24081123, 0x28102102, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00010204, 0x04091224, 0x08112204, 0x0c193326, 0x10210204, 0x14291224, 0x18312204, 0x1c393326,
	0x20000001, 0x24081021, 0x28102000, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00010306, 0x04091326, 0x08112306, 0x0c193326, 0x10210204, 0x14291224, 0x18312204, 0x1c393326,
	0x20000103, 0x24081123, 0x28102102, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00000000, 0x01201028, 0x02213018, 0x03012030, 0x04223138, 0x05022110, 0x06030120, 0x07231108,
	0x08042231, 0x09243219, 0x0a251229, 0x0b050201, 0x0c261309, 0x0d060321, 0x0e072311, 0x0f273339,
	0x10080422, 0x1128140a, 0x1229343a, 0x13092412, 0x142a351a, 0x150a2532, 0x160b0502, 0x172b152a,
	0x180c2613, 0x192c363b, 0x1a2d160b, 0x1b0d0623, 0x1c2e172b, 0x1d0e0703, 0x1e0f2733, 0x1f2f371b,
	0x20100804, 0x2130182c, 0x2231381c, 0x23112834, 0x2432393c, 0x25122914, 0x26130924, 0x2733190c,
	0x28142a35, 0x29343a1d, 0x2a351a2d, 0x2b150a05, 0x2c361b0d, 0x2d160b25, 0x2e172b15, 0x2f373b3d,
	0x30180c26, 0x31381c0e, 0x32393c3e, 0x33192c16, 0x343a3d1e, 0x351a2d36, 0x361b0d06, 0x373b1d2e,
	0x381c2e17, 0x393c3e3f, 0x3a3d1e0f, 0x3b1d0e27, 0x3c3e1f2f, 0x3d1e0f07, 0x3e1f2f37, 0x3f3f3f1f
};

static constexpr u32 CPU_CLOCK = 50000000;
static constexpr u32 PIXEL_CLOCK = 8000000 / 2;

tunit_machine::tunit_machine(tunit_roms roms, tunit_protection prot)
	: m_maincpu(machine_config(), "maincpu", nullptr, CPU_CLOCK)
	, m_video(m_machine, midtunit_video_device::kind::tunit)
	, m_sound(m_machine)
	, m_roms(std::move(roms))
	, m_prot(prot)
{
	m_roms.program.resize(0x100000, 0xff);
#ifdef NBAJAM_LOWMEM
	m_roms.gfx.resize(m_roms.gfx.size() + NBAJAM_GFX_GUARD, 0);   // in place, plus guard
#else
	m_roms.gfx.resize(0xc00000, 0);
#endif
	m_ram.assign(0x40000, 0);
	m_nvram.assign(0x2000, 0);
	m_palram.assign(0x8000, 0);
	std::fill(std::begin(m_pens), std::end(m_pens), 0);

	m_mixer.init(m_machine, 48000);

	// screen: from TMS340 registers (MAME: set_raw(PIXEL_CLOCK*2, 506, 100, 500, 289, 20, 274))
	screen_device &scr = m_machine.screen();
	scr.set_raw(PIXEL_CLOCK * 2, 506, 100, 500, 289, 20, 274);
	scr.m_update_ind16 = [this](screen_device &s, bitmap_ind16 &bm, const rectangle &clip) { m_maincpu.tms340x0_ind16(s, bm, clip); };
	scr.on_vblank = [this] { capture_frame(); };

	// main CPU
	m_machine.add_cpu(m_maincpu);
	m_maincpu.set_bus(AS_PROGRAM, this);
	m_maincpu.set_halt_on_reset(false);
	m_maincpu.set_pixel_clock(PIXEL_CLOCK);
	m_maincpu.set_pixels_per_clock(2);
	m_maincpu.set_screen(scr);
	m_maincpu.set_scanline_ind16_callback([this](screen_device &s, bitmap_ind16 &b, int line, const tms340x0_device::display_params *p) { m_video.scanline_update(s, b, line, p); });
	m_maincpu.set_shiftreg_in_callback([this](offs_t a, u16 *sr) { m_video.to_shiftreg(a, sr); });
	m_maincpu.set_shiftreg_out_callback([this](offs_t a, u16 *sr) { m_video.from_shiftreg(a, sr); });
	m_maincpu.start();
	m_maincpu.m_pc_hint = [this] { return offs_t(m_maincpu.state_pc()); };

#ifdef NBAJAM_LOWMEM
	m_video.start(m_roms.gfx.data(), m_roms.gfx.size() - NBAJAM_GFX_GUARD);
#else
	m_video.start(m_roms.gfx.data(), m_roms.gfx.size());
#endif
	m_video.dma_irq_cb = [this](int state) { m_maincpu.set_input_line(0, state); };

	m_sound.start(m_roms.snd_u3.data(), m_roms.snd_u12.data(), m_roms.snd_u13.data(), m_mixer);
#ifdef NBAJAM_LOWMEM
	// the sound board keeps its own copies
	std::vector<u8>().swap(m_roms.snd_u3); std::vector<u8>().swap(m_roms.snd_u12); std::vector<u8>().swap(m_roms.snd_u13);
#endif
	if (m_prot == tunit_protection::nbajam)
	{
		m_prot_table = nbajam_prot_values;
		m_sound.install_hidden_ram(0xfbaa, 0xfbd4);
	}
	else
	{
		m_prot_table = nbajamte_prot_values;
		m_sound.install_hidden_ram(0xfbec, 0xfc16);
	}

	m_frame.assign(size_t(m_vis_w) * m_vis_h, 0);
	reset();
}

tunit_machine::~tunit_machine() = default;

void tunit_machine::reset()
{
	m_maincpu.reset();
	m_sound.reset();
	m_sound.reset_write(1);
	m_sound.reset_write(0);
	m_fake_sound_state = 0;
}

double tunit_machine::refresh_hz() const
{
	return const_cast<running_machine &>(m_machine).screen().frame_period().as_hz();
}

void tunit_machine::run_frame()
{
	u64 start = m_frames;
	attotime limit = m_machine.m_basetime + attotime::from_msec(100);
	while (m_frames == start && m_machine.m_basetime < limit)
		m_machine.run_until(m_machine.m_basetime + attotime::from_usec(500));
}

void tunit_machine::capture_frame()
{
	screen_device &scr = m_machine.screen();
	const rectangle &vis = scr.visible_area();
	int w = vis.width(), h = vis.height();
	if (w != m_vis_w || h != m_vis_h)
	{
		m_vis_w = w; m_vis_h = h;
		m_frame.assign(size_t(w) * h, 0);
	}
	for (int y = 0; y < h; y++)
	{
		const u16 *src = &scr.m_bitmap.pix(vis.min_y + y, vis.min_x);
		u32 *dst = &m_frame[size_t(y) * w];
		for (int x = 0; x < w; x++) dst[x] = m_pens[src[x] & 0x7fff];
	}
	m_frames++;
}

void tunit_machine::set_inputs(const cabinet_inputs &in)
{
	auto pl = [&](int p) -> u16 {
		const auto &q = in.p[p];
		u16 v = 0;
		if (q.up) v |= 0x01;
		if (q.down) v |= 0x02;
		if (q.left) v |= 0x04;
		if (q.right) v |= 0x08;
		if (q.shoot) v |= 0x10;
		if (q.pass) v |= 0x20;
		if (q.turbo) v |= 0x40;
		return v;
	};
	m_in0 = u16(~(pl(0) | (pl(1) << 8)));
	m_in2 = u16(~(pl(2) | (pl(3) << 8)));
	u16 v1 = 0;
	if (in.p[0].coin) v1 |= 0x0001;
	if (in.p[1].coin) v1 |= 0x0002;
	if (in.p[0].start) v1 |= 0x0004;
	if (in.test) v1 |= 0x0010;
	if (in.p[1].start) v1 |= 0x0020;
	if (in.service) v1 |= 0x0040;
	if (in.p[2].coin) v1 |= 0x0080;
	if (in.p[3].coin) v1 |= 0x0100;
	if (in.p[2].start) v1 |= 0x0200;
	if (in.p[3].start) v1 |= 0x0400;
	if (in.volume_down) v1 |= 0x0800;
	if (in.volume_up) v1 |= 0x1000;
	m_in1 = u16(~v1);
}

void tunit_machine::set_dips(const dip_settings &d)
{
	u16 v = 0x7ffd;
	if (d.test_switch) v &= ~0x0001;
	if (d.powerup_test) v |= 0x0002;
	if (!d.video_clips) v &= ~0x0020;
	if (!d.four_players) v &= ~0x0080;
	v &= ~0x0c00;
	v |= (d.country == 1) ? 0x0800 : (d.country == 2) ? 0x0400 : 0x0c00;
	if (d.free_play) v = (v & ~0x7000) | 0x8000;   // free play on the dips, dip coinage source
	m_dsw = v;
}

void tunit_machine::take_audio(std::vector<float> &out)
{
	m_mixer.render(out);
}

std::vector<u8> tunit_machine::nvram() const
{
	std::vector<u8> d(m_nvram.size() * 2);
	for (size_t i = 0; i < m_nvram.size(); i++) { d[i * 2] = u8(m_nvram[i]); d[i * 2 + 1] = u8(m_nvram[i] >> 8); }
	return d;
}

void tunit_machine::set_nvram(const std::vector<u8> &d)
{
	for (size_t i = 0; i < m_nvram.size() && i * 2 + 1 < d.size(); i++)
		m_nvram[i] = u16(d[i * 2] | (d[i * 2 + 1] << 8));
}

u32 tunit_machine::pc() const
{
	return m_maincpu.state_pc();
}

std::string tunit_machine::status() const
{
	return string_format("frames=%llu dma=%llu snd: cmds=%u oki=%u ym=%u dac=%u", (unsigned long long)m_frames, (unsigned long long)m_video.m_dma_count, m_sound.m_cmds, m_sound.m_oki_cmds, m_sound.m_ym_writes, m_sound.m_dac_writes);
}

// ---------------------------------------------------------------------------
// protection
// ---------------------------------------------------------------------------
u16 tunit_machine::nbajam_prot_r()
{
	u16 const result = m_prot_queue[m_prot_index];
	if (m_prot_index < 4) m_prot_index++;
	return result;
}

void tunit_machine::nbajam_prot_w(offs_t offset, u16 data)
{
	int const table_index = (offset >> 6) & 0x7f;
	u32 const protval = m_prot_table[table_index];
	m_prot_queue[0] = data;
	m_prot_queue[1] = ((protval >> 24) & 0xff) << 9;
	m_prot_queue[2] = ((protval >> 16) & 0xff) << 9;
	m_prot_queue[3] = ((protval >> 8) & 0xff) << 9;
	m_prot_queue[4] = ((protval >> 0) & 0xff) << 9;
	m_prot_index = 0;
}

// ---------------------------------------------------------------------------
// memory map (addresses are TMS34010 bit addresses)
// ---------------------------------------------------------------------------
u64 tunit_machine::read(offs_t addr, u64 mem_mask) { return read16(addr, u16(mem_mask)); }
void tunit_machine::write(offs_t addr, u64 data, u64 mem_mask) { write16(addr, u16(data), u16(mem_mask)); }

#define IN_RANGE(a, s, e) ((a) >= (s) && (a) <= (e))
#define WOFF(a, s) (((a) - (s)) >> 4)

u16 tunit_machine::read16(offs_t a, u16 mask)
{
	if (a >= 0xff800000) return u16(m_roms.program[WOFF(a, 0xff800000) * 2] | (m_roms.program[WOFF(a, 0xff800000) * 2 + 1] << 8));
	if (IN_RANGE(a, 0x01000000, 0x013fffff)) return m_ram[WOFF(a, 0x01000000)];
	if (a < 0x00400000) return m_video.midtunit_vram_r(WOFF(a, 0));
	if (IN_RANGE(a, 0x02000000, 0x07ffffff)) return m_video.midtunit_gfxrom_r(WOFF(a, 0x02000000));
	if (m_prot == tunit_protection::nbajam)
	{
		if (IN_RANGE(a, 0x1b14020, 0x1b2503f)) return nbajam_prot_r();
	}
	else
	{
		if (IN_RANGE(a, 0x1b15f40, 0x1b37f5f) || IN_RANGE(a, 0x1b95f40, 0x1bb7f5f)) return nbajam_prot_r();
	}
	if (IN_RANGE(a, 0x01400000, 0x0141ffff)) return m_nvram[WOFF(a, 0x01400000) & 0x1fff];
	if (IN_RANGE(a, 0x01600000, 0x0160003f))
	{
		switch (WOFF(a, 0x01600000))
		{
			case 0: return m_in0;
			case 1: return m_in1;
			case 2: return m_in2;
			default: return m_dsw;
		}
	}
	if (IN_RANGE(a, 0x01800000, 0x0187ffff)) return m_palram[WOFF(a, 0x01800000)];
	if (IN_RANGE(a, 0x01a80000, 0x01a800ff)) return m_video.dma_r(WOFF(a, 0x01a80000));
	if (IN_RANGE(a, 0x01d00000, 0x01d0001f))
	{
		if (m_fake_sound_state) { m_fake_sound_state--; return 0; }
		return 0xffff;
	}
	if (IN_RANGE(a, 0x01d01020, 0x01d0103f)) return 0xffff;
	if (IN_RANGE(a, 0xc0000000, 0xc00001ff)) return m_maincpu.io_register_r(WOFF(a, 0xc0000000));
	if (IN_RANGE(a, 0x1f800000, 0x1fffffff)) return u16(m_roms.program[WOFF(a, 0x1f800000) * 2] | (m_roms.program[WOFF(a, 0x1f800000) * 2 + 1] << 8));
	return 0xffff;
}

void tunit_machine::write16(offs_t a, u16 data, u16 mask)
{
	if (IN_RANGE(a, 0x01000000, 0x013fffff)) { u16 &r = m_ram[WOFF(a, 0x01000000)]; r = u16((r & ~mask) | (data & mask)); return; }
	if (a < 0x00400000) { m_video.midtunit_vram_w(WOFF(a, 0), data, mask); return; }
	if (m_prot == tunit_protection::nbajam)
	{
		if (IN_RANGE(a, 0x1b14020, 0x1b2503f)) { nbajam_prot_w(WOFF(a, 0x1b14020), data); return; }
	}
	else
	{
		if (IN_RANGE(a, 0x1b15f40, 0x1b37f5f)) { nbajam_prot_w(WOFF(a, 0x1b15f40), data); return; }
		if (IN_RANGE(a, 0x1b95f40, 0x1bb7f5f)) { nbajam_prot_w(WOFF(a, 0x1b95f40), data); return; }
	}
	if (IN_RANGE(a, 0x01400000, 0x0141ffff)) { u16 &r = m_nvram[WOFF(a, 0x01400000) & 0x1fff]; r = u16((r & ~mask) | (data & mask)); return; }
	if (IN_RANGE(a, 0x01480000, 0x014fffff)) return; // cmos enable
	if (IN_RANGE(a, 0x01800000, 0x0187ffff))
	{
		offs_t o = WOFF(a, 0x01800000);
		u16 &r = m_palram[o];
		r = u16((r & ~mask) | (data & mask));
		// xRGB_555
		u32 rr = (r >> 10) & 0x1f, gg = (r >> 5) & 0x1f, bb = r & 0x1f;
		rr = (rr << 3) | (rr >> 2); gg = (gg << 3) | (gg >> 2); bb = (bb << 3) | (bb >> 2);
		m_pens[o] = 0xff000000 | (rr << 16) | (gg << 8) | bb;
		return;
	}
	if (IN_RANGE(a, 0x01a80000, 0x01a800ff)) { m_video.dma_w(WOFF(a, 0x01a80000), data, mask); return; }
	if (IN_RANGE(a, 0x01b00000, 0x01b0001f) || IN_RANGE(a, 0x01f00000, 0x01f0001f)) { m_video.midtunit_control_w(0, data, mask); return; }
	if (IN_RANGE(a, 0x01d01020, 0x01d0103f))
	{
		offs_t offset = WOFF(a, 0x01d01020);
		if (!offset) return;
		if ((mask & 0x00ff) && (mask & 0xff00))
		{
			m_sound.reset_write(~data & 0x100);
			m_sound.write(data & 0xff);
			m_fake_sound_state = 128;
		}
		return;
	}
	if (IN_RANGE(a, 0x01d81060, 0x01d8107f)) return; // watchdog
	if (IN_RANGE(a, 0xc0000000, 0xc00001ff)) { m_maincpu.io_register_w(WOFF(a, 0xc0000000), data, mask); return; }
}
