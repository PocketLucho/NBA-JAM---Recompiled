// wunit.h - Midway Wolf-unit (NBA Hangtime / Maximum Hangtime).
// Memory map, I/O and serial PIC follow MAME midwunit.cpp / midwunit_m.cpp /
// midwayic.cpp (BSD-3-Clause).
#pragma once
#include "machine.h"
#include "midway_video.h"
#include "dcs.h"
#include "tunit.h"   // tms34010_cpu

struct wunit_roms
{
	std::vector<u8> program;  // 0x100000 bytes, interleaved little-endian words
	std::vector<u8> gfx;      // up to 0x2000000 bytes (3 banks of 8MB)
	std::vector<u16> dcs;     // DCS data ROM, one ROM byte per word
};

class wunit_machine : public arcade_machine, private mem_bus
{
public:
	explicit wunit_machine(wunit_roms roms, int pic_upper = 528);
	~wunit_machine() override;

	void reset() override;
	void run_frame() override;
	int width() const override { return m_vis_w; }
	int height() const override { return m_vis_h; }
	const u32 *frame() const override { return m_frame.data(); }
	double refresh_hz() const override;
	void set_inputs(const cabinet_inputs &in) override;
	void set_dips(const dip_settings &d) override;
	void take_audio(std::vector<float> &out) override;
	std::vector<u8> nvram() const override;
	void set_nvram(const std::vector<u8> &d) override;
	std::string status() const override;
	u32 pc() const override { return m_maincpu.state_pc(); }
	u32 state_hash() const override { return fnv1a(m_ram.data(), m_ram.size() * 2); }
	u32 fingerprint() const override
	{
		u32 h = fnv1a(m_roms.program.data(), m_roms.program.size());
		return fnv1a(m_roms.dcs.data(), m_roms.dcs.size() * 2, h);
	}
	cpu_device *maincpu() override { return &m_maincpu; }
	cpu_device *soundcpu() override { return &m_dcs.cpu(); }

private:
	u64 read(offs_t addr, u64 mem_mask) override;
	void write(offs_t addr, u64 data, u64 mem_mask) override;
	u16 read16(offs_t a, u16 mask);
	void write16(offs_t a, u16 data, u16 mask);
	void capture_frame();

	// serial PIC simulation (MAME midway_serial_pic_device)
	void pic_generate(int upper);
	u8 pic_read() { m_pic_status = 1; return m_pic_buff; }
	void pic_write(u8 data);
	void pic_reset_w(int state) { if (state) { m_pic_idx = 0; m_pic_status = 0; m_pic_buff = 0; } }

	running_machine m_machine;
	tms34010_cpu m_maincpu;
	midtunit_video_device m_video;
	dcs_audio m_dcs;
	audio_mixer m_mixer;
	wunit_roms m_roms;

	std::vector<u16> m_ram;      // 0x40000 words
	std::vector<u16> m_nvram;    // 0x6000 words
	std::vector<u16> m_palram;   // 0x8000 words
	u32 m_pens[0x8000];
	u16 m_iodata[8] = {};
	u16 m_ports[4] = { 0xffff, 0xffff, 0x7f7d, 0xffff };  // IN0, IN1, DSW, IN2
	int m_cmos_write_enable = 0;

	u8 m_pic_data[16] = {};
	u8 m_pic_buff = 0, m_pic_idx = 0, m_pic_status = 0;
	int m_pic_upper;

	std::vector<u32> m_frame;
	int m_vis_w = 400, m_vis_h = 254;
	u64 m_frames = 0;
};
