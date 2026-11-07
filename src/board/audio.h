// audio.h - tiny time-based audio streams + mixer (MAME sound_stream in miniature)
#pragma once
#include "emu.h"

// A source generates mono samples at its native rate, on demand up to "now".
class stream_source
{
public:
	virtual ~stream_source() = default;
	void init(running_machine &m, double rate, float gain) { m_machine = &m; m_rate = rate; m_gain = gain; m_last = m.time(); }
	void set_rate(double r) { update(); m_rate = r; }
	double rate() const { return m_rate; }
	// bring the stream up to the current machine time
	void update()
	{
		attotime now = m_machine->time();
		if (now <= m_last) return;
		double exact = (now - m_last).as_double() * m_rate + m_frac;
		int n = int(exact);
		m_frac = exact - n;
		m_last = now;
		if (n <= 0) return;
		size_t old = m_buf.size();
		m_buf.resize(old + size_t(n));
		generate(&m_buf[old], n);
	}
	std::vector<float> m_buf;
	float m_gain = 1.0f;
protected:
	virtual void generate(float *out, int n) = 0;
	running_machine *m_machine = nullptr;
	double m_rate = 48000;
	double m_frac = 0;
	attotime m_last;
};

class audio_mixer
{
public:
	void init(running_machine &m, int out_rate) { m_machine = &m; m_out_rate = out_rate; m_last = m.time(); }
	void add(stream_source &s) { m_sources.push_back(&s); }
	// Render everything up to now into 'out' (appended), returns samples added.
	int render(std::vector<float> &out)
	{
		attotime now = m_machine->time();
		double exact = (now - m_last).as_double() * m_out_rate + m_frac;
		int n = int(exact);
		m_frac = exact - n;
		m_last = now;
		size_t base = out.size();
		out.resize(base + size_t(std::max(n, 0)), 0.0f);
		for (stream_source *s : m_sources)
		{
			s->update();
			const auto &b = s->m_buf;
			if (n > 0 && !b.empty())
			{
				double step = double(b.size()) / n;
				double pos = 0;
				for (int i = 0; i < n; i++, pos += step)
				{
					size_t i0 = size_t(pos);
					if (i0 >= b.size()) i0 = b.size() - 1;
					size_t i1 = std::min(i0 + 1, b.size() - 1);
					double f = pos - double(size_t(pos));
					out[base + i] += float(b[i0] * (1 - f) + b[i1] * f) * s->m_gain;
				}
			}
			s->m_buf.clear();
		}
		return n;
	}
private:
	running_machine *m_machine = nullptr;
	std::vector<stream_source *> m_sources;
	int m_out_rate = 48000;
	attotime m_last;
	double m_frac = 0;
};

// simple 8-bit DAC (AD7524 style), sample & hold
class dac8_source : public stream_source
{
public:
	void write(u8 data) { update(); m_level = (float(data) - 128.0f) / 128.0f; }
	void write16(u16 data) { update(); m_level = float(s16(data)) / 32768.0f; }
protected:
	void generate(float *out, int n) override { std::fill_n(out, n, m_level); }
	float m_level = 0;
};
