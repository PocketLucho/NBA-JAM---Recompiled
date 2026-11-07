// emu.cpp - minimal MAME-compatible framework implementation
#include "emu.h"

#include <cstdarg>

const attotime attotime::zero(0, 0);
const attotime attotime::never(attotime::MAX_SECONDS, 0);

int g_emu_log_enabled = 0;

std::string emu::detail::vformat(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) return std::string();
	if (size_t(n) < sizeof(buf)) return std::string(buf, size_t(n));
	std::string s(size_t(n) + 1, '\0');
	va_start(ap, fmt);
	vsnprintf(s.data(), s.size(), fmt, ap);
	va_end(ap);
	s.resize(size_t(n));
	return s;
}

void emu_log_line(const std::string &s)
{
	fputs(s.c_str(), stderr);
	if (s.empty() || s.back() != '\n') fputc('\n', stderr);
}

// --------------------------------------------------------------------------
// timers
// --------------------------------------------------------------------------
void emu_timer::adjust(attotime duration, s32 param, attotime period)
{
	m_param = param;
	m_period = period;
	m_start = m_machine.time();
	if (duration.is_never())
	{
		m_enabled = false;
		m_expire = attotime::never;
		return;
	}
	m_enabled = true;
	m_expire = m_start + duration;
	m_machine.timer_scheduled(this);
}

attotime emu_timer::remaining() const
{
	if (!m_enabled || m_expire.is_never()) return attotime::never;
	attotime now = m_machine.time();
	if (m_expire <= now) return attotime::zero;
	return m_expire - now;
}

attotime emu_timer::elapsed() const
{
	return m_machine.time() - m_start;
}

void emu_timer::fire()
{
	s32 param = m_param;
	if (!m_period.is_never() && !m_period.is_zero())
	{
		m_start = m_expire;
		m_expire = m_expire + m_period;
	}
	else
	{
		m_enabled = false;
		m_expire = attotime::never;
	}
	if (m_cb) m_cb(param);
}

void device_scheduler::synchronize(timer_expired_delegate cb, s32 param)
{
	if (cb) m_machine.timer_set(attotime::zero, std::move(cb), param);
	else abort_timeslice();
}

void device_scheduler::timer_set(attotime duration, timer_expired_delegate cb, s32 param)
{
	m_machine.timer_set(duration, std::move(cb), param);
}

void device_scheduler::abort_timeslice()
{
	if (m_machine.m_executing) m_machine.m_executing->abort_timeslice();
}

attotime device_scheduler::time() const { return m_machine.time(); }

// --------------------------------------------------------------------------
// screen
// --------------------------------------------------------------------------
void screen_device::set_raw(u32 pixclock, int htotal, int hbend, int hbstart, int vtotal, int vbend, int vbstart)
{
	rectangle vis(hbend, hbstart - 1, vbend, vbstart - 1);
	configure(htotal, vtotal, vis, attotime::from_ticks(u64(htotal) * vtotal, pixclock));
}

void screen_device::configure(int width, int height, const rectangle &visarea, attotime frame_period)
{
	m_width = width;
	m_height = height;
	m_visarea = visarea;
	m_frame_period = frame_period;
	m_scantime = frame_period.as_attoseconds() / height;
	m_pixeltime = m_scantime / width;
	if (m_bitmap.width() < width || m_bitmap.height() < height)
		m_bitmap.allocate(std::max(width, 1), std::max(height, 1));
	// reschedule vblank relative to the current position
	if (m_vbl_timer)
		m_vbl_timer->adjust(time_until_pos(m_visarea.max_y + 1));
}

int screen_device::vpos() const
{
	attoseconds_t delta = (m_machine.time() - m_frame_start).as_attoseconds();
	delta += m_pixeltime / 2;
	int vpos = int(delta / m_scantime);
	return (m_visarea.max_y + 1 + vpos) % m_height;
}

int screen_device::hpos() const
{
	attoseconds_t delta = (m_machine.time() - m_frame_start).as_attoseconds();
	delta += m_pixeltime / 2;
	int vpos = int(delta / m_scantime);
	return int((delta - attoseconds_t(vpos) * m_scantime) / m_pixeltime);
}

attotime screen_device::time_until_pos(int vpos, int hpos) const
{
	vpos -= m_visarea.max_y + 1;
	vpos += m_height;
	vpos %= m_height;
	attoseconds_t target = attoseconds_t(vpos) * m_scantime + attoseconds_t(hpos) * m_pixeltime;
	attoseconds_t cur = (m_machine.time() - m_frame_start).as_attoseconds();
	if (target <= cur + m_pixeltime / 2) target += m_frame_period.as_attoseconds();
	while (target <= cur) target += m_frame_period.as_attoseconds();
	return attotime(0, target - cur);
}

void screen_device::update_partial(int scanline)
{
	if (scanline < m_last_partial) return;
	if (scanline >= m_height) scanline = m_height - 1;
	for (int y = std::max(m_last_partial, 0); y <= scanline; y++)
	{
		if (y >= m_visarea.min_y && y <= m_visarea.max_y && m_update_ind16)
		{
			rectangle clip(m_visarea.min_x, m_visarea.max_x, y, y);
			m_update_ind16(*this, m_bitmap, clip);
		}
	}
	m_last_partial = scanline + 1;
}

void screen_device::begin_frame()
{
	// finish the previous frame
	update_partial(m_visarea.max_y);
	if (on_vblank) on_vblank();
	m_frame_start = m_machine.time();
	m_last_partial = 0;
	m_frame_number++;
}

// --------------------------------------------------------------------------
// devices
// --------------------------------------------------------------------------
device_t::device_t(const machine_config &, device_type, const char *tag, device_t *, u32 clock)
	: m_tag(tag ? tag : ""), m_clock(clock)
{
}

emu_timer *device_t::timer_alloc_raw(std::function<void(s32)> cb)
{
	return m_machine->timer_alloc(std::move(cb));
}

void cpu_device::set_input_line(int line, int state)
{
	if (line == INPUT_LINE_RESET)
	{
		if (state != CLEAR_LINE)
		{
			if (!m_in_reset) device_reset();
			m_in_reset = true;
			abort_timeslice();
		}
		else if (m_in_reset)
		{
			m_in_reset = false;
		}
		return;
	}
	if (line == INPUT_LINE_HALT)
	{
		m_input_state[line] = state;
		m_halted = (state != CLEAR_LINE);
		if (m_halted) abort_timeslice();
		return;
	}
	if (line < 0 || line >= 40) return;
	m_input_state[line] = state;
	execute_set_input(line, state == HOLD_LINE ? ASSERT_LINE : state);
	if (state != CLEAR_LINE && machine().m_executing && machine().m_executing != this)
		machine().scheduler().abort_timeslice();
}

int cpu_device::standard_irq_callback(int line, offs_t)
{
	if (line >= 0 && line < 40 && m_input_state[line] == HOLD_LINE)
	{
		m_input_state[line] = CLEAR_LINE;
		execute_set_input(line, CLEAR_LINE);
	}
	if (m_irq_ack) return m_irq_ack(line);
	return 0;
}

void cpu_device::abort_timeslice()
{
	if (!m_executing_now || !m_icountptr) return;
	int delta = *m_icountptr;
	if (delta <= 0) return;
	m_slice_cycles -= delta;
	*m_icountptr = 0;
}

u64 cpu_device::total_cycles() const
{
	if (m_executing_now && m_icountptr)
		return m_total_cycles_base + u64(s64(m_slice_cycles) - *m_icountptr);
	return m_total_cycles_base;
}

attotime cpu_device::local_time() const
{
	if (m_executing_now) return cycles_to_attotime(total_cycles());
	return m_localtime;
}

void cpu_device::run_until(const attotime &target)
{
	u64 target_cycles = attotime_to_cycles(target);
	if (target_cycles <= m_total_cycles_base)
		return;
	u64 want = target_cycles - m_total_cycles_base;
	if (want > 0x3fffffff) want = 0x3fffffff;
	if (suspended() || !m_icountptr)
	{
		m_total_cycles_base += want;
		m_localtime = cycles_to_attotime(m_total_cycles_base);
		return;
	}
	m_slice_cycles = int(want);
	*m_icountptr = int(want);
	m_executing_now = true;
	execute_run();
	m_executing_now = false;
	s64 executed = s64(m_slice_cycles) - *m_icountptr;
	if (executed < 0) executed = 0;
	m_total_cycles_base += u64(executed);
	m_localtime = cycles_to_attotime(m_total_cycles_base);
}

// --------------------------------------------------------------------------
// machine
// --------------------------------------------------------------------------
running_machine::running_machine()
	: m_scheduler(*this)
{
	m_screen = std::make_unique<screen_device>(*this);
	m_screen->m_vbl_timer = timer_alloc([this](s32) {
		m_screen->begin_frame();
		m_screen->m_vbl_timer->adjust(m_screen->time_until_pos(m_screen->visible_area().max_y + 1));
	});
}

running_machine::~running_machine() = default;

attotime running_machine::time() const
{
	if (m_executing) return m_executing->local_time();
	return m_basetime;
}

std::string running_machine::describe_context() const
{
	if (m_executing) return std::string(m_executing->tag());
	return "(no context)";
}

emu_timer *running_machine::timer_alloc(std::function<void(s32)> cb)
{
	m_timers.push_back(std::make_unique<emu_timer>(*this, std::move(cb)));
	return m_timers.back().get();
}

void running_machine::timer_set(attotime duration, std::function<void(s32)> cb, s32 param)
{
	// reuse a free one-shot if possible
	emu_timer *t = nullptr;
	for (auto &o : m_oneshots)
		if (!o->m_enabled) { t = o.get(); break; }
	if (!t)
	{
		m_oneshots.push_back(std::make_unique<emu_timer>(*this, nullptr));
		t = m_oneshots.back().get();
	}
	t->m_cb = std::move(cb);
	t->adjust(duration, param);
}

void running_machine::timer_scheduled(emu_timer *t)
{
	t->m_seq = ++m_timer_seq;
	// a timer due before the end of the running slice ends that slice
	if (m_executing && t->m_expire < m_executing->m_localtime + m_max_slice)
		m_executing->abort_timeslice();
}

emu_timer *running_machine::next_timer()
{
	emu_timer *best = nullptr;
	auto consider = [&](emu_timer *t) {
		if (!t->m_enabled) return;
		if (!best || t->m_expire < best->m_expire || (t->m_expire == best->m_expire && t->m_seq < best->m_seq))
			best = t;
	};
	for (auto &t : m_timers) consider(t.get());
	for (auto &t : m_oneshots) consider(t.get());
	return best;
}

void running_machine::run_until(const attotime &target)
{
	while (m_basetime < target)
	{
		// fire everything that is due
		for (;;)
		{
			emu_timer *t = next_timer();
			if (!t || t->m_expire > m_basetime) break;
			t->fire();
		}

		attotime next = target;
		if (emu_timer *t = next_timer())
			if (t->m_expire < next) next = t->m_expire;
		attotime cap = m_basetime + m_max_slice;
		if (cap < next) next = cap;
		if (next <= m_basetime) next = m_basetime + attotime(0, 1000);

		for (cpu_device *cpu : m_cpus)
		{
			m_executing = cpu;
			cpu->run_until(next);
			m_executing = nullptr;
			// a CPU that stopped early (synchronize / abort) shortens the slice
			if (cpu->m_localtime < next && cpu->m_localtime > m_basetime && !cpu->suspended())
				next = cpu->m_localtime;
		}
		m_basetime = next;
	}
	for (;;)
	{
		emu_timer *t = next_timer();
		if (!t || t->m_expire > m_basetime) break;
		t->fire();
	}
}
