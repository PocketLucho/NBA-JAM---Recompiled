// dcs.h - Williams/Midway DCS (rev 1, "DCS audio 8K") sound board: ADSP-2105 +
// banked data ROM + 16-bit DAC fed by SPORT1 autobuffer DMA.
// Logic follows MAME src/mame/shared/dcs.cpp (BSD-3-Clause, Aaron Giles et al.).
#pragma once
#include "emu.h"
#include "audio.h"
#include "cpu/adsp2100/adsp2100.h"

class adsp2105_cpu : public adsp2105_device
{
public:
	using adsp2105_device::adsp2105_device;
	u32 get_i(int n) const { return m_i[n]; }
	void set_i(int n, u32 v) { m_i[n] = v & 0x3fff; m_base[n] = m_i[n] & m_lmask[n]; }
	s32 get_m(int n) const { return m_m[n]; }
	u32 get_pc() const { return m_pc; }
	u32 get_l(int n) const { return m_l[n]; }
};

class dac_queue_source : public stream_source
{
public:
	void push(const s16 *s, int n) { update(); for (int i = 0; i < n; i++) m_q.push_back(s[i]); if (m_q.size() > 48000) m_q.erase(m_q.begin(), m_q.begin() + (m_q.size() - 24000)); }
	void enable(bool e) { update(); m_enabled = e; if (!e) m_q.clear(); }
protected:
	void generate(float *out, int n) override
	{
		for (int i = 0; i < n; i++)
		{
			if (m_enabled && m_rd < m_q.size()) m_last = m_q[m_rd++];
			out[i] = m_enabled ? m_last / 32768.0f : 0.0f;
		}
		if (m_rd) { m_q.erase(m_q.begin(), m_q.begin() + m_rd); m_rd = 0; }
	}
	std::vector<s16> m_q;
	size_t m_rd = 0;
	float m_last = 0;
	bool m_enabled = false;
};

class dcs_audio
{
public:
	explicit dcs_audio(running_machine &m);
	void start(std::vector<u16> bootrom, audio_mixer &mixer);   // bootrom: one ROM byte per word
	void reset() { dcs_reset(); }

	// host interface
	void data_w(u16 data);
	u16 data_r();
	int control_r() { return m_latch_control; }
	void reset_w(int state);

	adsp2105_cpu &cpu() { return m_cpu; }
	u32 m_dbg_in = 0, m_dbg_out = 0, m_dbg_irq = 0;

private:
	struct pbus : mem_bus { dcs_audio *d; u64 read(offs_t a, u64) override { return d->prog_r(a); } void write(offs_t a, u64 v, u64) override { d->prog_w(a, u32(v)); } } m_pbus;
	struct dbus : mem_bus { dcs_audio *d; u64 read(offs_t a, u64) override { return d->data_rd(a); } void write(offs_t a, u64 v, u64 m) override { d->data_wr(a, u16(v), u16(m)); } } m_dbus;
	u32 prog_r(offs_t a);
	void prog_w(offs_t a, u32 v);
	u16 data_rd(offs_t a);
	void data_wr(offs_t a, u16 v, u16 m);

	void dcs_reset();
	void dcs_boot();
	u16 adsp_control_r(offs_t offset);
	void adsp_control_w(offs_t offset, u16 data);
	void update_timer_count();
	void reset_timer();
	void timer_enable_callback(int state);
	void internal_timer_cb();
	void dcs_irq_cb();
	void recompute_sample_rate();
	void sound_tx_callback(offs_t offset, u32 data);
	void input_latch_ack_w();
	u16 input_latch_r();
	void output_latch_w(u16 data);
	void pulse_irq(int line);

	running_machine &m_machine;
	adsp2105_cpu m_cpu;
	dac_queue_source m_dac;

	std::vector<u16> m_bootrom;
	u32 m_banks = 1;
	u32 m_sounddata_bank = 0;
	u32 m_iprog[0x400] = {};
	u32 m_eprog[0x1800] = {};
	u16 m_dram[0x800] = {};
	u16 m_iram[0x200] = {};
	u16 m_control_regs[32] = {};

	u16 m_latch_control = 0;
	u16 m_input_data = 0, m_output_data = 0, m_pre_output_data = 0;
	int m_size = 0, m_incs = 0, m_ireg = 0, m_ireg_base = 0;
	int m_channels = 1;
	u16 m_progflags = 0;

	emu_timer *m_reg_timer = nullptr, *m_internal_timer = nullptr;
	bool m_timer_ignore = false;
	int m_timer_enable = 0, m_timer_scale = 1;
	u16 m_timer_period = 0, m_timer_start_count = 0;
	u64 m_timer_start_cycles = 0, m_timers_fired = 0;
};
