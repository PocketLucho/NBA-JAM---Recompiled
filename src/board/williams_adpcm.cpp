// williams_adpcm.cpp - see header
#include "williams_adpcm.h"

static constexpr XTAL ADPCM_MASTER_CLOCK = XTAL(8000000.0);
static constexpr XTAL ADPCM_FM_CLOCK = XTAL(3579545.0);

// ---------------------------------------------------------------------------
// OKI MSM6295 (MAME okim6295.cpp logic)
// ---------------------------------------------------------------------------
static const float s_oki_volume[16] = {
	0x20 / 32.0f, 0x16 / 32.0f, 0x10 / 32.0f, 0x0b / 32.0f, 0x08 / 32.0f, 0x06 / 32.0f, 0x04 / 32.0f, 0x03 / 32.0f,
	0x02 / 32.0f, 0, 0, 0, 0, 0, 0, 0 };

u8 okim6295_source::read()
{
	update();
	u8 result = 0xf0;
	for (int v = 0; v < 4; v++)
		if (m_voice[v].playing) result |= 1 << v;
	return result;
}

void okim6295_source::write(u8 command)
{
	if (m_command != -1)
	{
		int voicemask = command >> 4;
		update();
		for (int v = 0; v < 4; v++, voicemask >>= 1)
		{
			if (!(voicemask & 1)) continue;
			voice &vc = m_voice[v];
			if (vc.playing) continue;
			offs_t base = m_command * 8;
			offs_t start = (read_byte(base + 0) << 16) | (read_byte(base + 1) << 8) | read_byte(base + 2);
			start &= 0x3ffff;
			offs_t stop = (read_byte(base + 3) << 16) | (read_byte(base + 4) << 8) | read_byte(base + 5);
			stop &= 0x3ffff;
			if (start < stop)
			{
				vc.playing = true;
				vc.base_offset = start;
				vc.sample = 0;
				vc.count = 2 * (stop - start + 1);
				vc.adpcm.reset();
				vc.volume = s_oki_volume[command & 0x0f];
			}
		}
		m_command = -1;
	}
	else if (command & 0x80)
		m_command = command & 0x7f;
	else
	{
		update();
		int voicemask = command >> 3;
		for (int v = 0; v < 4; v++, voicemask >>= 1)
			if (voicemask & 1) m_voice[v].playing = false;
	}
}

void okim6295_source::generate(float *out, int n)
{
	std::fill_n(out, n, 0.0f);
	for (auto &vc : m_voice)
	{
		if (!vc.playing) continue;
		for (int i = 0; i < n; i++)
		{
			int nibble = read_byte(vc.base_offset + vc.sample / 2) >> (((vc.sample & 1) << 2) ^ 4);
			out[i] += float(vc.adpcm.clock(u8(nibble))) * vc.volume / 2048.0f;
			if (++vc.sample >= vc.count) { vc.playing = false; break; }
		}
	}
}

// ---------------------------------------------------------------------------
// YM2151 via ymfm
// ---------------------------------------------------------------------------
void ym2151_source::start(running_machine &m, u32 clock, float gain)
{
	m_clock = clock;
	init(m, double(m_chip.sample_rate(clock)), gain);
	for (int i = 0; i < 2; i++)
		m_timer[i] = m.timer_alloc([this, i](s32) { m_engine->engine_timer_expired(i); });
	m_chip.reset();
}

void ym2151_source::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
	if (tnum > 1) return;
	if (duration_in_clocks >= 0)
		m_timer[tnum]->adjust(attotime::from_ticks(u64(duration_in_clocks), m_clock));
	else
		m_timer[tnum]->enable(false);
}

void ym2151_source::generate(float *out, int n)
{
	ymfm::ym2151::output_data o;
	for (int i = 0; i < n; i++)
	{
		m_chip.generate(&o, 1);
		out[i] = float(o.data[0] + o.data[1]) * (0.5f / 32768.0f);
	}
}

// ---------------------------------------------------------------------------
// the board
// ---------------------------------------------------------------------------
williams_adpcm_sound::williams_adpcm_sound(running_machine &m)
	: m_machine(m), m_cpu(machine_config(), "adpcm:cpu", nullptr, ADPCM_MASTER_CLOCK.value() / 4)
{
	m_bus.s = this;
}

void williams_adpcm_sound::start(const u8 *u3, const u8 *u12, const u8 *u13, audio_mixer &mixer)
{
	m_rom.assign(0x50000, 0xff);
	std::memcpy(&m_rom[0x10000], u3, 0x20000);
	std::memcpy(&m_rom[0x30000], u3, 0x20000);
	m_okirom.assign(0x100000, 0);
	std::memcpy(&m_okirom[0x00000], u12, 0x80000);
	std::memcpy(&m_okirom[0x80000], u13, 0x80000);
	// MAME bank layout ("U12 @ 0x00000, U13 @ 0x80000")
	static const u32 bases[8] = { 0x40000, 0x40000, 0x20000, 0x00000, 0xe0000, 0xc0000, 0xa0000, 0x80000 };
	std::copy(bases, bases + 8, m_okibank_base);

	m_machine.add_cpu(m_cpu);
	m_cpu.set_bus(AS_PROGRAM, &m_bus);
	m_cpu.start();

	m_ym.start(m_machine, ADPCM_FM_CLOCK.value(), 0.10f * 1.0f);
	m_ym.irq_cb = [this](int state) { m_cpu.set_input_line(M6809_FIRQ_LINE, state); };
	m_dac.init(m_machine, 32000, 0.10f);
	m_oki.init(m_machine, ADPCM_MASTER_CLOCK.value() / 8 / 132.0, 0.15f);
	m_oki.read_byte = [this](offs_t a) -> u8 {
		a &= 0x3ffff;
		if (a < 0x20000) return m_okirom[m_okibank_base[m_okibank] + a];
		return m_okirom[0x60000 + (a - 0x20000)];
	};
	mixer.add(m_ym);
	mixer.add(m_dac);
	mixer.add(m_oki);

	m_sync_command_timer = m_machine.timer_alloc([this](s32 param) {
		m_latch = param & 0xff;
		if (!(param & 0x200))
		{
			m_cpu.set_input_line(M6809_IRQ_LINE, ASSERT_LINE);
			m_sound_int_state = 1;
		}
	});
	m_irq_clear_timer = m_machine.timer_alloc([this](s32) { m_sound_int_state = 0; });
}

void williams_adpcm_sound::reset()
{
	m_sound_int_state = 0;
	m_cpu.reset();
	m_cpu.set_input_line(M6809_FIRQ_LINE, CLEAR_LINE);
	m_cpu.set_input_line(M6809_IRQ_LINE, CLEAR_LINE);
	m_cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_sync_command_timer->adjust(attotime::never);
	m_irq_clear_timer->adjust(attotime::never);
	m_ym.reset();
	m_oki.reset();
}

void williams_adpcm_sound::write(u16 data)
{
	m_cmds++;
	m_sync_command_timer->adjust(attotime::zero, data);
}

void williams_adpcm_sound::reset_write(int state)
{
	if (state)
	{
		m_rombank = 0;
		m_sound_int_state = 0;
		m_cpu.set_input_line(M6809_FIRQ_LINE, CLEAR_LINE);
		m_cpu.set_input_line(M6809_IRQ_LINE, CLEAR_LINE);
		m_cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
		m_sync_command_timer->adjust(attotime::never);
		m_irq_clear_timer->adjust(attotime::never);
		m_cpu.set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	}
	else
		m_cpu.set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
}

u8 williams_adpcm_sound::cpu_read(u16 a)
{
	if (m_hidden_end && a >= m_hidden_start && a <= m_hidden_end)
		return m_hidden[a - m_hidden_start];
	if (a < 0x2000) return m_ram[a];
	if (a >= 0x4000 && a < 0xc000) return m_rom[0x10000 + m_rombank * 0x8000 + (a - 0x4000)];
	if (a >= 0xc000) return m_rom[0x10000 + 0x4000 + 7 * 0x8000 + (a - 0xc000)];
	switch (a & 0xfc00)
	{
		case 0x2400: return m_ym.read(a & 1);
		case 0x2c00: return m_oki.read();
		case 0x3000:
			m_cpu.set_input_line(M6809_IRQ_LINE, CLEAR_LINE);
			m_irq_clear_timer->adjust(attotime::from_usec(10));
			return m_latch;
	}
	return 0xff;
}

void williams_adpcm_sound::cpu_write(u16 a, u8 d)
{
	if (m_hidden_end && a >= m_hidden_start && a <= m_hidden_end)
	{
		m_hidden[a - m_hidden_start] = d;
		return;
	}
	if (a < 0x2000) { m_ram[a] = d; return; }
	switch (a & 0xfc00)
	{
		case 0x2000: m_rombank = d & 7; break;
		case 0x2400: m_ym_writes++; m_ym.write(a & 1, d); break;
		case 0x2800: m_dac_writes++; m_dac.write(d); break;
		case 0x2c00: m_oki_cmds++; m_oki.write(d); break;
		case 0x3400: m_oki.update(); m_okibank = d & 7; break;
		case 0x3c00: m_talkback = d; break;
	}
}
