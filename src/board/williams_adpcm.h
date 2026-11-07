// williams_adpcm.h - Williams/Midway ADPCM sound board (6809E + YM2151 + DAC + OKI6295)
// as used by the T-unit NBA Jam games. Logic follows MAME williamssound.cpp
// (BSD-3-Clause, Aaron Giles) and okim6295.cpp.
#pragma once
#include "emu.h"
#include "audio.h"
#include "cpu/m6809/m6809.h"
#include "sound/okiadpcm.h"
#include "ymfm/ymfm_opm.h"

class okim6295_source : public stream_source
{
public:
	std::function<u8(offs_t)> read_byte;
	u8 read();
	void write(u8 command);
	void reset() { update(); for (auto &v : m_voice) v.playing = false; m_command = -1; }
protected:
	void generate(float *out, int n) override;
private:
	struct voice
	{
		bool playing = false;
		offs_t base_offset = 0;
		u32 sample = 0, count = 0;
		float volume = 0;
		oki_adpcm_state adpcm;
	};
	voice m_voice[4];
	int m_command = -1;
};

class ym2151_source : public stream_source, public ymfm::ymfm_interface
{
public:
	ym2151_source() : m_chip(*this) {}
	void start(running_machine &m, u32 clock, float gain);
	u8 read(offs_t offset) { update(); return m_chip.read(offset & 1); }
	void write(offs_t offset, u8 data) { update(); m_chip.write(offset & 1, data); }
	void reset() { m_chip.reset(); }
	std::function<void(int)> irq_cb;
	// ymfm_interface
	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
	void ymfm_update_irq(bool asserted) override { if (irq_cb) irq_cb(asserted ? ASSERT_LINE : CLEAR_LINE); }
	void ymfm_sync_mode_write(uint8_t data) override { m_engine->engine_mode_write(data); }
	void ymfm_sync_check_interrupts() override { m_engine->engine_check_interrupts(); }
protected:
	void generate(float *out, int n) override;
private:
	ymfm::ym2151 m_chip;
	emu_timer *m_timer[2] = {};
	u32 m_clock = 0;
};

class williams_adpcm_sound
{
public:
	explicit williams_adpcm_sound(running_machine &m);
	void start(const u8 *cpu_rom /*0x20000 u3*/, const u8 *u12, const u8 *u13, audio_mixer &mixer);
	void reset();

	// interface to the main board
	void write(u16 data);
	void reset_write(int state);
	int irq_read() { return m_sound_int_state; }

	// sound-CPU-side protection RAM (hidden RAM) range
	void install_hidden_ram(u16 start, u16 end) { m_hidden_start = start; m_hidden_end = end; }

	mc6809e_device &cpu() { return m_cpu; }
	u32 m_cmds = 0, m_oki_cmds = 0, m_ym_writes = 0, m_dac_writes = 0;

private:
	struct bus_t : mem_bus
	{
		williams_adpcm_sound *s;
		u64 read(offs_t a, u64) override { return s->cpu_read(u16(a)); }
		void write(offs_t a, u64 d, u64) override { s->cpu_write(u16(a), u8(d)); }
	} m_bus;
	u8 cpu_read(u16 a);
	void cpu_write(u16 a, u8 d);

	running_machine &m_machine;
	mc6809e_device m_cpu;
	ym2151_source m_ym;
	okim6295_source m_oki;
	dac8_source m_dac;

	std::vector<u8> m_rom;    // 0x50000 region like MAME "cpu"
	std::vector<u8> m_okirom; // 0x100000
	u8 m_ram[0x2000] = {};
	u8 m_hidden[0x100] = {};
	u16 m_hidden_start = 0, m_hidden_end = 0;
	int m_rombank = 0, m_okibank = 0;
	u8 m_latch = 0, m_talkback = 0;
	int m_sound_int_state = 0;
	emu_timer *m_sync_command_timer = nullptr;
	emu_timer *m_irq_clear_timer = nullptr;
	u32 m_okibank_base[8] = {};
};
