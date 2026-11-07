// wunit.cpp - Midway Wolf-unit board (see header)
#include "wunit.h"

static constexpr u32 CPU_CLOCK = 50000000;
static constexpr u32 PIXEL_CLOCK = 8000000;

wunit_machine::wunit_machine(wunit_roms roms, int pic_upper)
	: m_maincpu(machine_config(), "maincpu", nullptr, CPU_CLOCK)
	, m_video(m_machine, midtunit_video_device::kind::wunit)
	, m_dcs(m_machine)
	, m_roms(std::move(roms))
	, m_pic_upper(pic_upper)
{
	m_roms.program.resize(0x100000, 0xff);
	m_roms.gfx.resize(0x2000000, 0);
	m_ram.assign(0x40000, 0);
	m_nvram.assign(0x6000, 0);
	m_palram.assign(0x8000, 0);
	std::fill(std::begin(m_pens), std::end(m_pens), 0);
	m_mixer.init(m_machine, 48000);

	screen_device &scr = m_machine.screen();
	scr.set_raw(PIXEL_CLOCK, 506, 101, 501, 289, 20, 274);
	scr.m_update_ind16 = [this](screen_device &s, bitmap_ind16 &bm, const rectangle &clip) { m_maincpu.tms340x0_ind16(s, bm, clip); };
	scr.on_vblank = [this] { capture_frame(); };

	m_machine.add_cpu(m_maincpu);
	m_maincpu.set_bus(AS_PROGRAM, this);
	m_maincpu.set_halt_on_reset(false);
	m_maincpu.set_pixel_clock(PIXEL_CLOCK);
	m_maincpu.set_pixels_per_clock(1);
	m_maincpu.set_screen(scr);
	m_maincpu.set_scanline_ind16_callback([this](screen_device &s, bitmap_ind16 &b, int line, const tms340x0_device::display_params *p) { m_video.scanline_update(s, b, line, p); });
	m_maincpu.set_shiftreg_in_callback([this](offs_t a, u16 *sr) { m_video.to_shiftreg(a, sr); });
	m_maincpu.set_shiftreg_out_callback([this](offs_t a, u16 *sr) { m_video.from_shiftreg(a, sr); });
	m_maincpu.start();

	m_video.set_gfx_rom_large(true);
	m_video.start(m_roms.gfx.data(), m_roms.gfx.size());
	m_video.dma_irq_cb = [this](int state) { m_maincpu.set_input_line(0, state); };

	m_dcs.start(m_roms.dcs, m_mixer);

	m_frame.assign(size_t(m_vis_w) * m_vis_h, 0);
	reset();
}

wunit_machine::~wunit_machine() = default;

void wunit_machine::reset()
{
	m_maincpu.reset();
	m_dcs.reset_w(0);
	m_dcs.reset_w(1);
	pic_generate(m_pic_upper);
	pic_reset_w(1);
}

double wunit_machine::refresh_hz() const
{
	return const_cast<running_machine &>(m_machine).screen().frame_period().as_hz();
}

void wunit_machine::run_frame()
{
	u64 start = m_frames;
	attotime limit = m_machine.m_basetime + attotime::from_msec(100);
	while (m_frames == start && m_machine.m_basetime < limit)
		m_machine.run_until(m_machine.m_basetime + attotime::from_usec(500));
}

void wunit_machine::capture_frame()
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

void wunit_machine::set_inputs(const cabinet_inputs &in)
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
	m_ports[0] = u16(~(pl(0) | (pl(1) << 8)));
	m_ports[1] = u16(~(pl(2) | (pl(3) << 8)));
	u16 v = 0;
	if (in.p[0].coin) v |= 0x0001;
	if (in.p[1].coin) v |= 0x0002;
	if (in.p[0].start) v |= 0x0004;
	if (in.test) v |= 0x0010;
	if (in.p[1].start) v |= 0x0020;
	if (in.service) v |= 0x0040;
	if (in.p[2].coin) v |= 0x0080;
	if (in.p[3].coin) v |= 0x0100;
	if (in.p[2].start) v |= 0x0200;
	if (in.p[3].start) v |= 0x0400;
	if (in.volume_down) v |= 0x0800;
	if (in.volume_up) v |= 0x1000;
	m_ports[3] = u16(~v);
}

void wunit_machine::set_dips(const dip_settings &d)
{
	u16 v = 0x7f7d;
	if (d.test_switch) v &= ~0x0001;
	if (d.powerup_test) v |= 0x0002;
	v &= ~0x0c00;
	v |= (d.country == 1) ? 0x0800 : (d.country == 2) ? 0x0400 : 0x0c00;
	if (d.free_play) v = (v & ~0x7000) | 0x8000;
	m_ports[2] = v;
}

void wunit_machine::take_audio(std::vector<float> &out) { m_mixer.render(out); }

std::vector<u8> wunit_machine::nvram() const
{
	std::vector<u8> d(m_nvram.size() * 2);
	for (size_t i = 0; i < m_nvram.size(); i++) { d[i * 2] = u8(m_nvram[i]); d[i * 2 + 1] = u8(m_nvram[i] >> 8); }
	return d;
}

void wunit_machine::set_nvram(const std::vector<u8> &d)
{
	for (size_t i = 0; i < m_nvram.size() && i * 2 + 1 < d.size(); i++)
		m_nvram[i] = u16(d[i * 2] | (d[i * 2 + 1] << 8));
}

std::string wunit_machine::status() const
{
	auto &d = const_cast<dcs_audio &>(m_dcs);
	return string_format("frames=%llu dma=%llu dcs in=%u out=%u dacirq=%u cyc=%llu", (unsigned long long)m_frames, (unsigned long long)m_video.m_dma_count,
		d.m_dbg_in, d.m_dbg_out, d.m_dbg_irq, (unsigned long long)d.cpu().get_pc());
}

// ---------------------------------------------------------------------------
// serial PIC (simulation; MAME midway_serial_pic_device)
// ---------------------------------------------------------------------------
void wunit_machine::pic_generate(int upper)
{
	int year = 1996, month = 12, day = 11;
	u32 serial_number = 123450 + u32(upper) * 1000000;
	u8 d[9];
	d[0] = (serial_number / 100000000) % 10; d[1] = (serial_number / 10000000) % 10; d[2] = (serial_number / 1000000) % 10;
	d[3] = (serial_number / 100000) % 10; d[4] = (serial_number / 10000) % 10; d[5] = (serial_number / 1000) % 10;
	d[6] = (serial_number / 100) % 10; d[7] = (serial_number / 10) % 10; d[8] = serial_number % 10;
	m_pic_data[12] = u8(m_machine.rand());
	m_pic_data[13] = u8(m_machine.rand());
	m_pic_data[14] = 0;
	m_pic_data[15] = 0;
	u32 temp = 0x174 * (year - 1980) + 0x1f * (month - 1) + day;
	m_pic_data[10] = (temp >> 8) & 0xff;
	m_pic_data[11] = temp & 0xff;
	temp = d[4] + d[7] * 10 + d[1] * 100;
	temp = (temp + 5 * m_pic_data[13]) * 0x1bcd + 0x1f3f0;
	m_pic_data[7] = temp & 0xff; m_pic_data[8] = (temp >> 8) & 0xff; m_pic_data[9] = (temp >> 16) & 0xff;
	temp = d[6] + d[8] * 10 + d[0] * 100 + d[2] * 10000;
	temp = (temp + 2 * m_pic_data[13] + m_pic_data[12]) * 0x107f + 0x71e259;
	m_pic_data[3] = temp & 0xff; m_pic_data[4] = (temp >> 8) & 0xff; m_pic_data[5] = (temp >> 16) & 0xff; m_pic_data[6] = (temp >> 24) & 0xff;
	temp = d[5] * 10 + d[3] * 100;
	temp = (temp + m_pic_data[12]) * 0x245 + 0x3d74;
	m_pic_data[0] = temp & 0xff; m_pic_data[1] = (temp >> 8) & 0xff; m_pic_data[2] = (temp >> 16) & 0xff;
}

void wunit_machine::pic_write(u8 data)
{
	m_pic_status = (data >> 4) & 1;
	if (!m_pic_status)
	{
		if (data & 0x0f)
			m_pic_buff = data & 0xf;
		else
			m_pic_buff = m_pic_data[m_pic_idx++ % sizeof(m_pic_data)];
	}
}

// ---------------------------------------------------------------------------
// memory map (bit addresses)
// ---------------------------------------------------------------------------
u64 wunit_machine::read(offs_t addr, u64 mem_mask) { return read16(addr, u16(mem_mask)); }
void wunit_machine::write(offs_t addr, u64 data, u64 mem_mask) { write16(addr, u16(data), u16(mem_mask)); }

#define IN_RANGE(a, s, e) ((a) >= (s) && (a) <= (e))
#define WOFF(a, s) (((a) - (s)) >> 4)

u16 wunit_machine::read16(offs_t a, u16 mask)
{
	if (a >= 0xff800000) { offs_t w = WOFF(a, 0xff800000); return u16(m_roms.program[w * 2] | (m_roms.program[w * 2 + 1] << 8)); }
	if (IN_RANGE(a, 0x01000000, 0x013fffff)) return m_ram[WOFF(a, 0x01000000)];
	if (a < 0x00400000) return m_video.midtunit_vram_r(WOFF(a, 0));
	if (IN_RANGE(a, 0x02000000, 0x06ffffff)) return m_video.midwunit_gfxrom_r(WOFF(a, 0x02000000));
	if (IN_RANGE(a, 0x01400000, 0x0145ffff)) return m_nvram[WOFF(a, 0x01400000)];
	if (IN_RANGE(a, 0x01600000, 0x0160001f)) return pic_read();
	if (IN_RANGE(a, 0x01680000, 0x0168001f)) return m_dcs.data_r() & 0xff;
	if (IN_RANGE(a, 0x01800000, 0x0187ffff))
	{
		offs_t o = WOFF(a, 0x01800000) % 16 % 8;
		if (o < 4) return m_ports[o];
		if (o == 4) return u16((m_pic_status << 12) | m_dcs.control_r());
		return 0xffff;
	}
	if (IN_RANGE(a, 0x01880000, 0x018fffff)) return m_palram[WOFF(a, 0x01880000) & 0x7fff];
	if (IN_RANGE(a, 0x01a00000, 0x01a000ff) || IN_RANGE(a, 0x01a80000, 0x01a800ff)) return m_video.dma_r(WOFF(a & ~0x80000u, 0x01a00000));
	if (IN_RANGE(a, 0x01b00000, 0x01b0001f)) return m_video.midwunit_control_r();
	if (IN_RANGE(a, 0xc0000000, 0xc00001ff)) return m_maincpu.io_register_r(WOFF(a, 0xc0000000));
	return 0xffff;
}

void wunit_machine::write16(offs_t a, u16 data, u16 mask)
{
	if (IN_RANGE(a, 0x01000000, 0x013fffff)) { u16 &r = m_ram[WOFF(a, 0x01000000)]; r = u16((r & ~mask) | (data & mask)); return; }
	if (a < 0x00400000) { m_video.midtunit_vram_w(WOFF(a, 0), data, mask); return; }
	if (IN_RANGE(a, 0x01400000, 0x0145ffff))
	{
		if (m_cmos_write_enable)
		{
			u16 &r = m_nvram[WOFF(a, 0x01400000)];
			r = u16((r & ~mask) | (data & mask));
			m_cmos_write_enable = 0;
		}
		return;
	}
	if (IN_RANGE(a, 0x01480000, 0x014fffff)) { m_cmos_write_enable = 1; return; }
	if (IN_RANGE(a, 0x01600000, 0x0160001f)) { if (WOFF(a, 0x01600000) == 0 && (mask & 0xff)) pic_write(u8(data)); return; }
	if (IN_RANGE(a, 0x01680000, 0x0168001f)) { if (WOFF(a, 0x01680000) == 0 && (mask & 0xff)) m_dcs.data_w(data & 0xff); return; }
	if (IN_RANGE(a, 0x01800000, 0x0187ffff))
	{
		offs_t o = WOFF(a, 0x01800000) % 16 % 8;
		u16 nw = u16((m_iodata[o] & ~mask) | (data & mask));
		if (o == 1)
		{
			m_dcs.reset_w(~nw & 0x10);
			pic_reset_w(nw & 0x20);
		}
		m_iodata[o] = nw;
		return;
	}
	if (IN_RANGE(a, 0x01880000, 0x018fffff))
	{
		offs_t o = WOFF(a, 0x01880000) & 0x7fff;
		u16 &r = m_palram[o];
		r = u16((r & ~mask) | (data & mask));
		u32 rr = (r >> 10) & 0x1f, gg = (r >> 5) & 0x1f, bb = r & 0x1f;
		rr = (rr << 3) | (rr >> 2); gg = (gg << 3) | (gg >> 2); bb = (bb << 3) | (bb >> 2);
		m_pens[o] = 0xff000000 | (rr << 16) | (gg << 8) | bb;
		return;
	}
	if (IN_RANGE(a, 0x01a00000, 0x01a000ff) || IN_RANGE(a, 0x01a80000, 0x01a800ff)) { m_video.dma_w(WOFF(a & ~0x80000u, 0x01a00000), data, mask); return; }
	if (IN_RANGE(a, 0x01b00000, 0x01b0001f)) { m_video.midwunit_control_w(0, data, mask); return; }
	if (IN_RANGE(a, 0xc0000000, 0xc00001ff)) { m_maincpu.io_register_w(WOFF(a, 0xc0000000), data, mask); return; }
}
