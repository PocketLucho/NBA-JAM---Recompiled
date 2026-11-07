// dcs.cpp - see header (port of the DCS rev1 parts of MAME dcs.cpp)
#include "dcs.h"
#include <cstdio>
#include <cstdlib>

static constexpr u16 LCTRL_OUTPUT_EMPTY = 0x400;
static constexpr u16 LCTRL_INPUT_EMPTY = 0x800;

enum
{
	IDMA_CONTROL_REG = 0, BDMA_INT_ADDR_REG, BDMA_EXT_ADDR_REG, BDMA_CONTROL_REG, BDMA_WORD_COUNT_REG,
	PROG_FLAG_DATA_REG, PROG_FLAG_CONTROL_REG,
	S1_AUTOBUF_REG = 15, S1_RFSDIV_REG, S1_SCLKDIV_REG, S1_CONTROL_REG, S0_AUTOBUF_REG, S0_RFSDIV_REG,
	S0_SCLKDIV_REG, S0_CONTROL_REG, S0_MCTXLO_REG, S0_MCTXHI_REG, S0_MCRXLO_REG, S0_MCRXHI_REG,
	TIMER_SCALE_REG, TIMER_COUNT_REG, TIMER_PERIOD_REG, WAITSTATES_REG, SYSCONTROL_REG
};

dcs_audio::dcs_audio(running_machine &m)
	: m_machine(m), m_cpu(machine_config(), "dcs", nullptr, 10000000)
{
	m_pbus.d = this;
	m_dbus.d = this;
}

void dcs_audio::start(std::vector<u16> bootrom, audio_mixer &mixer)
{
	m_bootrom = std::move(bootrom);
	if (m_bootrom.size() < 0x1000) m_bootrom.resize(0x1000, 0xff);
	m_banks = u32(m_bootrom.size() / 0x1000);

	m_machine.add_cpu(m_cpu);
	m_cpu.set_bus(AS_PROGRAM, &m_pbus);
	m_cpu.set_bus(AS_DATA, &m_dbus);
	m_cpu.sport_tx().set([this](offs_t o, u32 d, u32) { sound_tx_callback(o, d); });
	m_cpu.timer_fired().set([this](int s) { timer_enable_callback(s); });
	m_cpu.start();

	m_reg_timer = m_machine.timer_alloc([this](s32) { dcs_irq_cb(); });
	m_internal_timer = m_machine.timer_alloc([this](s32) { internal_timer_cb(); });

	// operators set the DCS volume low (factory default 30/255) because the cabinet amp
	// adds the gain; compensate so it plays at a level similar to the T-Unit games
	m_dac.init(m_machine, 31250, 6.0f);
	mixer.add(m_dac);
	dcs_reset();
}

// ---------------------------------------------------------------------------
// memory maps (DCS 8K)
// ---------------------------------------------------------------------------
u32 dcs_audio::prog_r(offs_t a)
{
	a &= 0x3fff;
	if (a < 0x400) return m_iprog[a];
	if (a >= 0x800 && a < 0x2000) return m_eprog[a - 0x800];
	if (a >= 0x3000 && a < 0x3004) return u32(input_latch_r()) << 8;
	return 0;
}

void dcs_audio::prog_w(offs_t a, u32 v)
{
	a &= 0x3fff;
	if (a < 0x400) { m_iprog[a] = v & 0xffffff; return; }
	if (a >= 0x800 && a < 0x2000) { m_eprog[a - 0x800] = v & 0xffffff; return; }
	if (a >= 0x3000 && a < 0x3004) { output_latch_w(u16(v >> 8)); return; }
}

u16 dcs_audio::data_rd(offs_t a)
{
	a &= 0x3fff;
	if (a < 0x800) return m_dram[a];
	if (a < 0x2000) return u16(m_eprog[a - 0x800] >> 8);
	if (a < 0x3000) return m_bootrom[(m_sounddata_bank % m_banks) * 0x1000 + (a - 0x2000)];
	if (a >= 0x3400 && a < 0x3404) return input_latch_r();
	if (a >= 0x3800 && a < 0x3a00) return m_iram[a - 0x3800];
	if (a >= 0x3fe0) return adsp_control_r(a - 0x3fe0);
	return 0xffff;
}

void dcs_audio::data_wr(offs_t a, u16 v, u16 m)
{
	a &= 0x3fff;
	if (a < 0x800) { m_dram[a] = u16((m_dram[a] & ~m) | (v & m)); return; }
	if (a < 0x2000)
	{
		u32 &p = m_eprog[a - 0x800];
		u16 val = u16(p >> 8);
		val = u16((val & ~m) | (v & m));
		p = (u32(val) << 8) | (p & 0xff);
		return;
	}
	if (a == 0x3000) { m_sounddata_bank = v & 0x7ff; return; }
	if (a >= 0x3400 && a < 0x3404) { output_latch_w(v); return; }
	if (a >= 0x3800 && a < 0x3a00) { m_iram[a - 0x3800] = v; return; }
	if (a >= 0x3fe0) { adsp_control_w(a - 0x3fe0, v); return; }
}

// ---------------------------------------------------------------------------
// reset / boot
// ---------------------------------------------------------------------------
void dcs_audio::dcs_boot()
{
	const u16 *base = &m_bootrom[(m_sounddata_bank * 0x1000) % m_bootrom.size()];
	u8 buffer[0x1000];
	for (int i = 0; i < 0x1000; i++) buffer[i] = u8(base[i]);
	m_cpu.load_boot_data(buffer, m_iprog);
}

void dcs_audio::dcs_reset()
{
	m_sounddata_bank = 0;
	m_size = m_incs = m_ireg = 0;
	std::fill(std::begin(m_control_regs), std::end(m_control_regs), 0);
	m_cpu.set_input_line(ADSP2105_IRQ0, CLEAR_LINE);
	m_cpu.set_input_line(ADSP2105_IRQ1, CLEAR_LINE);
	m_cpu.set_input_line(ADSP2105_IRQ2, CLEAR_LINE);
	m_latch_control |= LCTRL_INPUT_EMPTY;
	m_latch_control |= LCTRL_OUTPUT_EMPTY;
	dcs_boot();
	m_timer_ignore = false;
	m_timer_enable = 0;
	m_timer_scale = 1;
	m_internal_timer->enable(false);
}

void dcs_audio::reset_w(int state)
{
	if (!state)
	{
		m_machine.scheduler().synchronize([this](s32) { dcs_reset(); });
		m_cpu.set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	}
	else
		m_cpu.set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
}

// ---------------------------------------------------------------------------
// host latches
// ---------------------------------------------------------------------------
void dcs_audio::data_w(u16 data)
{
	m_dbg_in++;
	m_machine.scheduler().synchronize([this](s32 d) {
		m_cpu.set_input_line(ADSP2105_IRQ2, ASSERT_LINE);
		m_latch_control &= ~LCTRL_INPUT_EMPTY;
		m_input_data = u16(d);
	}, data);
}

void dcs_audio::input_latch_ack_w()
{
	m_latch_control |= LCTRL_INPUT_EMPTY;
	m_cpu.set_input_line(ADSP2105_IRQ2, CLEAR_LINE);
}

u16 dcs_audio::input_latch_r()
{
	input_latch_ack_w();     // auto-ack (non-RAM based board)
	return m_input_data;
}

void dcs_audio::output_latch_w(u16 data)
{
	m_pre_output_data = data;
	m_dbg_out++;
	m_machine.scheduler().synchronize([this](s32) {
		m_latch_control &= ~LCTRL_OUTPUT_EMPTY;
		m_output_data = m_pre_output_data;
	});
}

u16 dcs_audio::data_r()
{
	m_latch_control |= LCTRL_OUTPUT_EMPTY;    // auto ack
	return m_output_data;
}

// ---------------------------------------------------------------------------
// timers
// ---------------------------------------------------------------------------
void dcs_audio::update_timer_count()
{
	if (!m_timer_enable) return;
	u64 elapsed_cycles = m_cpu.total_cycles() - m_timer_start_cycles;
	u64 elapsed_clocks = elapsed_cycles / m_timer_scale;
	if (elapsed_clocks < u64(m_timer_start_count) + 1)
	{
		m_timer_start_count -= u16(elapsed_clocks);
		m_control_regs[TIMER_COUNT_REG] = m_timer_start_count;
	}
	else
	{
		elapsed_clocks -= m_timer_start_count + 1;
		u64 periods = elapsed_clocks / (u64(m_timer_period) + 1);
		elapsed_clocks -= periods * (u64(m_timer_period) + 1);
		m_timer_start_count = u16(m_timer_period - elapsed_clocks);
		m_control_regs[TIMER_COUNT_REG] = m_timer_start_count;
	}
}

void dcs_audio::internal_timer_cb()
{
	m_timers_fired++;
	s64 target = s64(m_timer_start_cycles + m_timer_scale * (m_timer_start_count + 1 + m_timers_fired * (u64(m_timer_period) + 1)));
	target -= s64(m_cpu.total_cycles());
	if (!m_timer_ignore && (m_timer_period > 10 || m_timer_scale > 1))
		m_internal_timer->adjust(m_cpu.cycles_to_attotime(u64(std::max<s64>(target, 1))));
	m_cpu.set_input_line(ADSP2105_TIMER, ASSERT_LINE);
	m_cpu.set_input_line(ADSP2105_TIMER, CLEAR_LINE);
}

void dcs_audio::reset_timer()
{
	if (!m_timer_enable) return;
	m_timer_start_cycles = m_cpu.total_cycles();
	m_timers_fired = 0;
	if (!m_timer_ignore &&
		prog_r(0x18) == 0x0c0030 && prog_r(0x19) == 0x804828 && prog_r(0x1a) == 0x904828 &&
		prog_r(0x1b) == 0x0c0020 && prog_r(0x1c) == 0x0a001f)
		m_timer_ignore = true;
	if (!m_timer_ignore)
		m_internal_timer->adjust(m_cpu.cycles_to_attotime(u64(m_timer_scale) * (m_timer_start_count + 1)));
}

void dcs_audio::timer_enable_callback(int state)
{
	if (state)
	{
		m_timer_enable = state;
		reset_timer();
	}
	else
	{
		update_timer_count();
		m_timer_enable = state;
		m_internal_timer->enable(false);
	}
}

// ---------------------------------------------------------------------------
// control registers / SPORT1 autobuffer DAC
// ---------------------------------------------------------------------------
u16 dcs_audio::adsp_control_r(offs_t offset)
{
	switch (offset)
	{
		case PROG_FLAG_DATA_REG:
		{
			u16 r = u16((m_control_regs[PROG_FLAG_CONTROL_REG] & m_control_regs[PROG_FLAG_DATA_REG]) | (m_progflags & ~m_control_regs[PROG_FLAG_CONTROL_REG]));
			m_progflags ^= 0x6;
			return r;
		}
		case TIMER_COUNT_REG:
			update_timer_count();
			return m_control_regs[offset];
		default:
			return m_control_regs[offset];
	}
}

void dcs_audio::adsp_control_w(offs_t offset, u16 data)
{
	m_control_regs[offset] = data;
	switch (offset)
	{
		case SYSCONTROL_REG:
			if (data & 0x0200)
			{
				m_cpu.set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
				m_cpu.set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
				dcs_boot();
				m_control_regs[SYSCONTROL_REG] = 0;
			}
			if ((data & 0x0800) == 0)
			{
				m_dac.enable(false);
				m_reg_timer->enable(false);
			}
			break;
		case S1_AUTOBUF_REG:
			if ((data & 0x0002) == 0)
			{
				m_dac.enable(false);
				m_reg_timer->enable(false);
			}
			break;
		case TIMER_SCALE_REG:
			data = (data & 0xff) + 1;
			if (data != m_timer_scale)
			{
				update_timer_count();
				m_timer_scale = data;
				reset_timer();
			}
			break;
		case TIMER_COUNT_REG:
			m_timer_start_count = data;
			reset_timer();
			break;
		case TIMER_PERIOD_REG:
			if (data != m_timer_period)
			{
				update_timer_count();
				m_timer_period = data;
				reset_timer();
			}
			break;
	}
}

void dcs_audio::pulse_irq(int line)
{
	m_cpu.set_input_line(line, ASSERT_LINE);
	m_machine.timer_set(attotime::from_nsec(100), [this, line](s32) { m_cpu.set_input_line(line, CLEAR_LINE); }, 0);
}

void dcs_audio::dcs_irq_cb()
{
	int reg = int(m_cpu.get_i(m_ireg));
	int const count = m_size / (2 * (m_incs ? m_incs : 1));
	s16 buffer[0x800];
	for (int i = 0; i < count && i < 0x800; i++)
	{
		buffer[i] = s16(data_rd(offs_t(reg)));
		reg += m_incs;
	}
	m_dac.push(buffer, std::min(count, 0x800));
	m_dbg_irq++;

	m_ireg_base = int(m_cpu.get_ibase(m_ireg));
	if (reg >= m_ireg_base + m_size)
	{
		reg = m_ireg_base;
		pulse_irq(ADSP2105_IRQ1);
	}
	m_cpu.set_i(m_ireg, u32(reg));
}

void dcs_audio::recompute_sample_rate()
{
	attotime sample_period;
	if (m_control_regs[S1_CONTROL_REG] & 0x4000)
	{
		sample_period = attotime::from_hz(m_cpu.clock()) * (2 * (m_control_regs[S1_SCLKDIV_REG] + 1));
		sample_period = sample_period * u32(16 * m_channels);
	}
	else
		sample_period = attotime::from_hz(31250);

	m_dac.set_rate(sample_period.as_hz());
	m_dac.enable(true);
	if (m_incs)
	{
		attotime period = (sample_period * u32(m_size)) / u32(2 * m_channels * m_incs);
		m_reg_timer->adjust(period, 0, period);
	}
}

void dcs_audio::sound_tx_callback(offs_t offset, u32)
{
	if (offset != 1) return;
	if ((m_control_regs[SYSCONTROL_REG] & 0x0800) && (m_control_regs[S1_AUTOBUF_REG] & 0x0002))
	{
		m_ireg = (m_control_regs[S1_AUTOBUF_REG] >> 9) & 7;
		int mreg = (m_control_regs[S1_AUTOBUF_REG] >> 7) & 3;
		mreg |= m_ireg & 0x04;
		int lreg = m_ireg;
		u16 source = u16(m_cpu.get_i(m_ireg));
		m_incs = m_cpu.get_m(mreg);
		m_size = int(m_cpu.get_l(lreg));
		source &= ~0xf;
		m_cpu.set_i(m_ireg, source);
		m_ireg_base = source;
		recompute_sample_rate();
		return;
	}
	m_dac.enable(false);
	m_reg_timer->enable(false);
}
