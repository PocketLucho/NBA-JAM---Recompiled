// emu.h - minimal MAME-compatible framework ("minimame") for NBA JAM Recomp.
//
// Lets selected MAME device sources (TMS34010, M6809, ADSP-21xx cores) compile
// unmodified, so the CPU emulation is exactly MAME's. Only the framework parts
// those cores touch are provided: time, timers, scheduler, CPU execute interface,
// address spaces (routed to board-provided buses), screen timing and callbacks.
#pragma once
#ifndef __EMU_H__
#define __EMU_H__

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <ostream>
#include <sstream>

// --------------------------------------------------------------------------
// basic types
// --------------------------------------------------------------------------
using u8 = uint8_t;   using s8 = int8_t;
using u16 = uint16_t; using s16 = int16_t;
using u32 = uint32_t; using s32 = int32_t;
using u64 = uint64_t; using s64 = int64_t;
using offs_t = u32;
using pen_t = u32;
using attoseconds_t = s64;

#define ATTR_COLD
#define ATTR_HOT
#define ATTR_FORCE_INLINE
#define ATTR_PRINTF(a,b)
#define LSB_FIRST 1
#define UNEXPECTED(x) (x)
#define EXPECTED(x) (x)

enum endianness_t { ENDIANNESS_LITTLE, ENDIANNESS_BIG };
constexpr endianness_t ENDIANNESS_NATIVE = ENDIANNESS_LITTLE;

union PAIR16 { struct { u8 l, h; } b; u16 w; };
union PAIR {
	struct { u8 l, h, h2, h3; } b;
	struct { u16 l, h; } w;
	struct { s8 l, h, h2, h3; } sb;
	struct { s16 l, h; } sw;
	u32 d; s32 sd;
};

template <typename T, typename U> constexpr T BIT(T x, U n) noexcept { return (x >> n) & T(1); }
template <typename T, typename U, typename V> constexpr T BIT(T x, U n, V w) { return (x >> n) & ((T(1) << w) - 1); }

namespace util {
template <typename T> constexpr std::make_signed_t<T> sext(T value, unsigned width) noexcept
{
	return std::make_signed_t<T>(value << (8 * sizeof(T) - width)) >> (8 * sizeof(T) - width);
}
class disasm_interface
{
public:
	class data_buffer
	{
	public:
		// generic reader (used by tools); addresses as the CPU uses them
		std::function<u64(offs_t, int)> reader;
		u8 r8(offs_t a) const { return reader ? u8(reader(a, 8)) : 0; }
		u16 r16(offs_t a) const { return reader ? u16(reader(a, 16)) : 0; }
		u32 r32(offs_t a) const { return reader ? u32(reader(a, 32)) : 0; }
		u64 r64(offs_t a) const { return reader ? reader(a, 64) : 0; }
	};
	enum : u32 {
		FLAGS_MASK = 0xf0000000, LENGTHMASK = 0x0000ffff, SUPPORTED = 0x80000000,
		STEP_OUT = 0x40000000, STEP_OVER = 0x20000000, STEP_COND = 0x10000000,
		OVERINSTMASK = 0x0f000000, OVERINSTSHIFT = 24, ROTATION = 0
	};
	static constexpr u32 step_over_extra(u32 x) { return x << OVERINSTSHIFT; }
	virtual ~disasm_interface() = default;
	virtual u32 opcode_alignment() const = 0;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) = 0;
	virtual u32 interface_flags() const { return 0; }
	virtual u32 page_address_bits() const { return 0; }
};
} // namespace util

inline s64 mul_32x32(s32 a, s32 b) { return s64(a) * s64(b); }
inline u64 mulu_32x32(u32 a, u32 b) { return u64(a) * u64(b); }
inline u8 count_leading_zeros_32(u32 v) { return v ? u8(__builtin_clz(v)) : 32; }
inline u8 count_leading_ones_32(u32 v) { return count_leading_zeros_32(~v); }
inline unsigned population_count_32(u32 v) { return __builtin_popcount(v); }

#define ACCESSING_BITS_0_7      ((mem_mask & 0x000000ffU) != 0)
#define ACCESSING_BITS_8_15     ((mem_mask & 0x0000ff00U) != 0)
#define ACCESSING_BITS_0_15     ((mem_mask & 0x0000ffffU) != 0)
#define ACCESSING_BITS_16_31    ((mem_mask & 0xffff0000U) != 0)
#define ACCESSING_BITS_16_23    ((mem_mask & 0x00ff0000U) != 0)
#define ACCESSING_BITS_24_31    ((mem_mask & 0xff000000U) != 0)
template <typename T, typename U, typename V> inline void COMBINE_DATA_impl(T *var, U data, V mem_mask) { *var = T((*var & ~mem_mask) | (data & mem_mask)); }
#define COMBINE_DATA(varptr) COMBINE_DATA_impl(varptr, data, mem_mask)

enum { CLEAR_LINE = 0, ASSERT_LINE, HOLD_LINE };
enum { INPUT_LINE_IRQ0 = 0, INPUT_LINE_NMI = 32, INPUT_LINE_RESET = 33, INPUT_LINE_HALT = 34 };
enum { STATE_GENPC = -1, STATE_GENPCBASE = -2, STATE_GENSP = -3, STATE_GENFLAGS = -4 };
enum { AS_PROGRAM = 0, AS_DATA = 1, AS_IO = 2, AS_OPCODES = 3 };

// --------------------------------------------------------------------------
// formatting / logging
// --------------------------------------------------------------------------
namespace emu::detail {
template <typename T> inline auto fmt_arg(const T &v)
{
	if constexpr (std::is_same_v<std::decay_t<T>, std::string>) return v.c_str();
	else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) return std::string(v).c_str();
	else if constexpr (std::is_enum_v<T>) return int(v);
	else return v;
}
std::string vformat(const char *fmt, ...);
} // namespace emu::detail

template <typename... Args> inline std::string string_format(const char *fmt, Args &&... args)
{
	return emu::detail::vformat(fmt, emu::detail::fmt_arg(args)...);
}
namespace util {
template <typename Stream, typename... Args> inline void stream_format(Stream &s, const char *fmt, Args &&... args)
{
	s << string_format(fmt, std::forward<Args>(args)...);
}
}
extern int g_emu_log_enabled;
void emu_log_line(const std::string &s);
template <typename... Args> inline void osd_printf_debug(const char *, Args &&...) {}
template <typename... Args> inline void popmessage(const char *, Args &&...) {}
template <typename... Args> [[noreturn]] inline void fatalerror(const char *fmt, Args &&... args)
{
	throw std::runtime_error(string_format(fmt, std::forward<Args>(args)...));
}

// FUNC(x) -> member pointer + name, like MAME
#define FUNC(x) &x, #x
#define NAME(x) x, #x

// --------------------------------------------------------------------------
// attotime
// --------------------------------------------------------------------------
constexpr attoseconds_t ATTOSECONDS_PER_SECOND = 1000000000000000000LL;
constexpr attoseconds_t ATTOSECONDS_PER_MILLISECOND = ATTOSECONDS_PER_SECOND / 1000;
constexpr attoseconds_t ATTOSECONDS_PER_MICROSECOND = ATTOSECONDS_PER_SECOND / 1000000;
constexpr attoseconds_t ATTOSECONDS_PER_NANOSECOND = ATTOSECONDS_PER_SECOND / 1000000000;
inline constexpr attoseconds_t HZ_TO_ATTOSECONDS(double x) { return attoseconds_t(double(ATTOSECONDS_PER_SECOND) / x); }
inline constexpr attoseconds_t ATTOSECONDS_IN_USEC(s64 x) { return x * ATTOSECONDS_PER_MICROSECOND; }
inline constexpr attoseconds_t ATTOSECONDS_IN_NSEC(s64 x) { return x * ATTOSECONDS_PER_NANOSECOND; }

class XTAL
{
public:
	constexpr explicit XTAL(double v) : m_v(v) {}
	constexpr double dvalue() const { return m_v; }
	constexpr u32 value() const { return u32(m_v + 1e-3); }
	constexpr XTAL operator/(int d) const { return XTAL(m_v / d); }
	constexpr XTAL operator*(int d) const { return XTAL(m_v * d); }
	constexpr operator double() const { return m_v; }
private:
	double m_v;
};
constexpr XTAL operator""_MHz_XTAL(long double v) { return XTAL(double(v * 1e6)); }
constexpr XTAL operator""_MHz_XTAL(unsigned long long v) { return XTAL(double(v) * 1e6); }
constexpr XTAL operator""_kHz_XTAL(long double v) { return XTAL(double(v * 1e3)); }
constexpr XTAL operator""_kHz_XTAL(unsigned long long v) { return XTAL(double(v) * 1e3); }

class attotime
{
public:
	static constexpr s32 MAX_SECONDS = 1000000000;
	constexpr attotime() : m_sec(0), m_atto(0) {}
	constexpr attotime(s32 secs, attoseconds_t attos) : m_sec(secs), m_atto(attos) {}

	static const attotime zero;
	static const attotime never;

	bool is_zero() const { return m_sec == 0 && m_atto == 0; }
	bool is_never() const { return m_sec >= MAX_SECONDS; }
	double as_double() const { return double(m_sec) + double(m_atto) * 1e-18; }
	attoseconds_t as_attoseconds() const
	{
		if (m_sec == 0) return m_atto;
		if (m_sec >= 9) return ATTOSECONDS_PER_SECOND * 9; // clamp
		return m_sec * ATTOSECONDS_PER_SECOND + m_atto;
	}
	double as_hz() const { return is_zero() ? 0.0 : 1.0 / as_double(); }
	u64 as_ticks(u32 freq) const
	{
		u64 fracticks = u64(m_atto / 1000000000LL) * freq / 1000000000ULL;
		return u64(m_sec) * freq + fracticks;
	}
	u64 as_ticks(const XTAL &x) const { return as_ticks(x.value()); }
	s32 seconds() const { return m_sec; }
	attoseconds_t attoseconds() const { return m_atto; }

	static attotime from_double(double d)
	{
		if (d >= MAX_SECONDS) return never;
		s32 s = s32(std::floor(d));
		return attotime(s, attoseconds_t((d - s) * double(ATTOSECONDS_PER_SECOND)));
	}
	static attotime from_ticks(u64 ticks, u32 freq)
	{
		if (freq == 0) return never;
		attoseconds_t per = ATTOSECONDS_PER_SECOND / freq;
		s32 secs = s32(ticks / freq);
		u64 rem = ticks % freq;
		return attotime(secs, attoseconds_t(rem) * per);
	}
	static attotime from_ticks(u64 ticks, const XTAL &x) { return from_ticks(ticks, x.value()); }
	static attotime from_seconds(s32 s) { return attotime(s, 0); }
	static attotime from_msec(s64 m) { return attotime(s32(m / 1000), attoseconds_t(m % 1000) * ATTOSECONDS_PER_MILLISECOND); }
	static attotime from_usec(s64 u) { return attotime(s32(u / 1000000), attoseconds_t(u % 1000000) * ATTOSECONDS_PER_MICROSECOND); }
	static attotime from_nsec(s64 n) { return attotime(s32(n / 1000000000), attoseconds_t(n % 1000000000) * ATTOSECONDS_PER_NANOSECOND); }
	static attotime from_hz(double hz) { return hz > 0 ? from_double(1.0 / hz) : never; }
	static attotime from_hz(u32 hz) { return hz > 0 ? attotime(0, ATTOSECONDS_PER_SECOND / hz) : never; }
	static attotime from_hz(int hz) { return from_hz(u32(hz)); }
	static attotime from_hz(const XTAL &x) { return from_hz(x.dvalue()); }

	attotime operator+(const attotime &r) const
	{
		if (is_never() || r.is_never()) return never;
		s32 s = m_sec + r.m_sec; attoseconds_t a = m_atto + r.m_atto;
		if (a >= ATTOSECONDS_PER_SECOND) { a -= ATTOSECONDS_PER_SECOND; s++; }
		if (s >= MAX_SECONDS) return never;
		return attotime(s, a);
	}
	attotime operator-(const attotime &r) const
	{
		if (is_never()) return never;
		if (r.is_never()) return zero;
		s32 s = m_sec - r.m_sec; attoseconds_t a = m_atto - r.m_atto;
		if (a < 0) { a += ATTOSECONDS_PER_SECOND; s--; }
		return attotime(s, a);
	}
	attotime &operator+=(const attotime &r) { return *this = *this + r; }
	attotime &operator-=(const attotime &r) { return *this = *this - r; }
	attotime operator*(u32 f) const
	{
		if (is_never()) return never;
		if (f == 0) return zero;
		// split attos into 1e9 halves to avoid overflow
		u64 lo = u64(m_atto % 1000000000LL) * f;
		u64 hi = u64(m_atto / 1000000000LL) * f;
		u64 secs = u64(m_sec) * f;
		hi += lo / 1000000000ULL; lo %= 1000000000ULL;
		secs += hi / 1000000000ULL; hi %= 1000000000ULL;
		if (secs >= MAX_SECONDS) return never;
		return attotime(s32(secs), attoseconds_t(hi) * 1000000000LL + attoseconds_t(lo));
	}
	attotime operator/(u32 d) const
	{
		if (is_never()) return never;
		if (d == 0) return never;
		double v = as_double() / d;
		u64 secs = u64(m_sec) / d;
		u64 rem = u64(m_sec) % d;
		// rem * 1e18 / d + atto / d, computed carefully
		long double extra = (long double)rem * (long double)ATTOSECONDS_PER_SECOND / d;
		attoseconds_t a = attoseconds_t(extra) + m_atto / d;
		(void)v;
		if (a >= ATTOSECONDS_PER_SECOND) { a -= ATTOSECONDS_PER_SECOND; secs++; }
		return attotime(s32(secs), a);
	}
	bool operator==(const attotime &r) const { return m_sec == r.m_sec && m_atto == r.m_atto; }
	bool operator!=(const attotime &r) const { return !(*this == r); }
	bool operator<(const attotime &r) const { return m_sec < r.m_sec || (m_sec == r.m_sec && m_atto < r.m_atto); }
	bool operator<=(const attotime &r) const { return !(r < *this); }
	bool operator>(const attotime &r) const { return r < *this; }
	bool operator>=(const attotime &r) const { return !(*this < r); }
private:
	s32 m_sec;
	attoseconds_t m_atto;
};

// --------------------------------------------------------------------------
// forward decls / misc device plumbing
// --------------------------------------------------------------------------
class device_t;
class cpu_device;
class running_machine;
class screen_device;
class emu_timer;
struct machine_config {};
using device_type = const char *;
#define DECLARE_DEVICE_TYPE(Type, Class) class Class; extern const device_type Type;
#define DEFINE_DEVICE_TYPE(Type, Class, ShortName, FullName) const device_type Type = ShortName;

#define TIMER_CALLBACK_MEMBER(name) void name(s32 param)

class timer_expired_delegate : public std::function<void(s32)>
{
public:
	using base = std::function<void(s32)>;
	timer_expired_delegate() = default;
	timer_expired_delegate(std::nullptr_t) {}
	template <typename F, typename = std::enable_if_t<std::is_invocable_v<F, s32>>> timer_expired_delegate(F &&f) : base(std::forward<F>(f)) {}
	template <typename T, typename O> timer_expired_delegate(void (T::*fn)(s32), const char *, O *obj)
		: base([obj, fn](s32 p) { (static_cast<T *>(obj)->*fn)(p); }) {}
};

// --- generic delegates & device callbacks ------------------------------------
template <typename Sig> class device_delegate;
template <typename R, typename... A> class device_delegate<R(A...)>
{
public:
	device_delegate() = default;
	template <typename T> explicit device_delegate(T &) {}
	template <typename F> void set(F &&f) { m_f = std::forward<F>(f); }
	template <typename Obj, typename C> void set(Obj &obj, R (C::*mf)(A...), const char *)
	{ C *o = &obj; m_f = [o, mf](A... a) { return (o->*mf)(a...); }; }
	template <typename C> void set(R (C::*mf)(A...), const char *) { (void)mf; }
	void resolve() {}
	bool isnull() const { return !m_f; }
	explicit operator bool() const { return bool(m_f); }
	R operator()(A... a) const { return m_f(a...); }
private:
	std::function<R(A...)> m_f;
};

namespace emu::detail {
template <typename Self> struct devcb_array_helper
{
	template <unsigned N> struct array : std::array<Self, N>
	{
		array() = default;
		template <typename T> explicit array(T &) {}
		template <typename T, typename D> array(T &, D) {}
		void resolve_all() {}
		template <typename D> void resolve_all_safe(D) {}
	};
};
} // namespace emu::detail

template <typename D> struct devcb_binder
{
	D *d;
	template <typename F> D &set(F &&f) { return d->set(std::forward<F>(f)); }
};

class devcb_write_line : public emu::detail::devcb_array_helper<devcb_write_line>
{
public:
	devcb_write_line() = default;
	template <typename T> explicit devcb_write_line(T &) {}
	devcb_binder<devcb_write_line> bind() { return { this }; }
	template <typename F> devcb_write_line &set(F &&f) { m_f = std::forward<F>(f); return *this; }
	void resolve() {}
	void resolve_safe() {}
	bool isunset() const { return !m_f; }
	bool isnull() const { return !m_f; }
	void operator()(int state) const { if (m_f) m_f(state); }
	void operator()(offs_t, int state) const { if (m_f) m_f(state); }
private:
	std::function<void(int)> m_f;
};

class devcb_read_line : public emu::detail::devcb_array_helper<devcb_read_line>
{
public:
	devcb_read_line() = default;
	template <typename T> explicit devcb_read_line(T &) {}
	template <typename T, typename D> devcb_read_line(T &, D d) : m_dflt(int(d)) {}
	devcb_binder<devcb_read_line> bind() { return { this }; }
	template <typename F> devcb_read_line &set(F &&f) { m_f = std::forward<F>(f); return *this; }
	void resolve() {}
	template <typename D> void resolve_safe(D d) { m_dflt = int(d); }
	bool isunset() const { return !m_f; }
	bool isnull() const { return !m_f; }
	int operator()() const { return m_f ? m_f() : m_dflt; }
private:
	std::function<int()> m_f;
	int m_dflt = 0;
};

template <typename T> class devcb_write : public emu::detail::devcb_array_helper<devcb_write<T>>
{
public:
	devcb_write() = default;
	template <typename D> explicit devcb_write(D &) {}
	devcb_binder<devcb_write> bind() { return { this }; }
	template <typename F> devcb_write &set(F &&f)
	{
		if constexpr (std::is_invocable_v<F, offs_t, T, T>) m_f = std::forward<F>(f);
		else if constexpr (std::is_invocable_v<F, offs_t, T>) m_f = [f](offs_t o, T d, T) { f(o, d); };
		else m_f = [f](offs_t, T d, T) { f(d); };
		return *this;
	}
	void resolve() {}
	void resolve_safe() {}
	bool isunset() const { return !m_f; }
	bool isnull() const { return !m_f; }
	void operator()(T data) const { if (m_f) m_f(0, data, T(~T(0))); }
	void operator()(offs_t o, T data, T mask = T(~T(0))) const { if (m_f) m_f(o, data, mask); }
private:
	std::function<void(offs_t, T, T)> m_f;
};

template <typename T> class devcb_read : public emu::detail::devcb_array_helper<devcb_read<T>>
{
public:
	devcb_read() = default;
	template <typename D> explicit devcb_read(D &) {}
	template <typename D, typename V> devcb_read(D &, V d) : m_dflt(T(d)) {}
	devcb_binder<devcb_read> bind() { return { this }; }
	template <typename F> devcb_read &set(F &&f)
	{
		if constexpr (std::is_invocable_v<F, offs_t, T>) m_f = std::forward<F>(f);
		else if constexpr (std::is_invocable_v<F, offs_t>) m_f = [f](offs_t o, T) { return T(f(o)); };
		else m_f = [f](offs_t, T) { return T(f()); };
		return *this;
	}
	void resolve() {}
	template <typename V> void resolve_safe(V d) { m_dflt = T(d); }
	bool isunset() const { return !m_f; }
	bool isnull() const { return !m_f; }
	T operator()(offs_t o = 0, T mask = T(~T(0))) const { return m_f ? m_f(o, mask) : m_dflt; }
private:
	std::function<T(offs_t, T)> m_f;
	T m_dflt = T(0);
};

using devcb_write8 = devcb_write<u8>;
using devcb_write16 = devcb_write<u16>;
using devcb_write32 = devcb_write<u32>;
using devcb_read8 = devcb_read<u8>;
using devcb_read16 = devcb_read<u16>;
using devcb_read32 = devcb_read<u32>;

// --------------------------------------------------------------------------
// timers & scheduler
// --------------------------------------------------------------------------
class emu_timer
{
public:
	emu_timer(running_machine &m, std::function<void(s32)> cb) : m_machine(m), m_cb(std::move(cb)) {}
	void adjust(attotime duration, s32 param = 0, attotime period = attotime::never);
	void reset(attotime duration = attotime::never) { adjust(duration, m_param, m_period); }
	bool enable(bool en = true) { bool old = m_enabled; m_enabled = en; return old; }
	bool enabled() const { return m_enabled; }
	attotime remaining() const;
	attotime expire() const { return m_expire; }
	attotime start() const { return m_start; }
	attotime elapsed() const;
	s32 param() const { return m_param; }
	void set_param(s32 p) { m_param = p; }
	attotime period() const { return m_period; }

	// scheduler internal
	void fire();
	running_machine &m_machine;
	std::function<void(s32)> m_cb;
	attotime m_start, m_expire = attotime::never, m_period = attotime::never;
	s32 m_param = 0;
	bool m_enabled = false;
	u64 m_seq = 0;
};

class device_scheduler
{
public:
	explicit device_scheduler(running_machine &m) : m_machine(m) {}
	void synchronize(timer_expired_delegate cb = nullptr, s32 param = 0);
	void timer_set(attotime duration, timer_expired_delegate cb, s32 param = 0);
	void perfect_quantum(attotime) {}
	void add_quantum(attotime, attotime) {}
	void abort_timeslice();
	void trigger(int) {}
	void boost_interleave(attotime, attotime) {}
	attotime time() const;
private:
	running_machine &m_machine;
};

// --------------------------------------------------------------------------
// address spaces
// --------------------------------------------------------------------------
// A bus is supplied by the board for each CPU address space. Addresses are in
// the CPU's own address units; accesses are always native-width.
struct mem_bus
{
	virtual ~mem_bus() = default;
	virtual u64 read(offs_t addr, u64 mem_mask) = 0;
	virtual void write(offs_t addr, u64 data, u64 mem_mask) = 0;
};

namespace emu::detail {
template <int Width, int AddrShift> struct mem_geom
{
	static constexpr int native_bytes = 1 << Width;
	// address units per byte (AddrShift>0: bits; <0: words)
	static constexpr offs_t bytes_to_units(offs_t b) { return AddrShift >= 0 ? (b << AddrShift) : (b >> -AddrShift); }
	static constexpr offs_t native_step = (AddrShift >= 0 ? (offs_t(native_bytes) << AddrShift) : offs_t(native_bytes >> -AddrShift));
	static constexpr offs_t align(offs_t a) { return native_step > 1 ? (a & ~(native_step - 1)) : a; }
};
}

template <int Width, int AddrShift, endianness_t Endian> struct memory_access_specific
{
	using G = emu::detail::mem_geom<Width, AddrShift>;
	mem_bus *bus = nullptr;
	static constexpr u64 native_mask = (Width >= 3) ? ~u64(0) : ((u64(1) << (8 << Width)) - 1);

	u64 rn(offs_t a, u64 m) { return bus->read(G::align(a), m); }
	void wn(offs_t a, u64 d, u64 m) { bus->write(G::align(a), d, m); }

	// generic: access of 'bytes' size at address a (in units)
	template <int Bytes> u64 rd(offs_t a, u64 mask)
	{
		constexpr int nb = G::native_bytes;
		if constexpr (Bytes == nb) return rn(a, mask) & native_mask;
		else if constexpr (Bytes < nb)
		{
			// sub-native access
			offs_t base = G::align(a);
			unsigned byteoff = unsigned((AddrShift >= 0) ? ((a - base) >> AddrShift) : 0);
			unsigned sh = (Endian == ENDIANNESS_LITTLE) ? byteoff * 8 : (nb - Bytes - byteoff) * 8;
			u64 v = rn(base, mask << sh);
			return (v >> sh) & mask;
		}
		else
		{
			// wider than native: compose
			constexpr int n = Bytes / nb;
			u64 r = 0;
			for (int i = 0; i < n; i++)
			{
				int part = (Endian == ENDIANNESS_LITTLE) ? i : (n - 1 - i);
				u64 pm = (mask >> (part * nb * 8)) & native_mask;
				if (pm) r |= (rn(a + G::native_step * i, pm) & native_mask) << (part * nb * 8);
			}
			return r;
		}
	}
	template <int Bytes> void wr(offs_t a, u64 data, u64 mask)
	{
		constexpr int nb = G::native_bytes;
		if constexpr (Bytes == nb) wn(a, data, mask);
		else if constexpr (Bytes < nb)
		{
			offs_t base = G::align(a);
			unsigned byteoff = unsigned((AddrShift >= 0) ? ((a - base) >> AddrShift) : 0);
			unsigned sh = (Endian == ENDIANNESS_LITTLE) ? byteoff * 8 : (nb - Bytes - byteoff) * 8;
			wn(base, data << sh, mask << sh);
		}
		else
		{
			constexpr int n = Bytes / nb;
			for (int i = 0; i < n; i++)
			{
				int part = (Endian == ENDIANNESS_LITTLE) ? i : (n - 1 - i);
				u64 pm = (mask >> (part * nb * 8)) & native_mask;
				if (pm) wn(a + G::native_step * i, (data >> (part * nb * 8)) & native_mask, pm);
			}
		}
	}

	u8  read_byte(offs_t a) { return u8(rd<1>(a, 0xff)); }
	u16 read_word(offs_t a) { return u16(rd<2>(a, 0xffff)); }
	u16 read_word(offs_t a, u16 m) { return u16(rd<2>(a, m)); }
	u32 read_dword(offs_t a) { return u32(rd<4>(a, 0xffffffff)); }
	u32 read_dword(offs_t a, u32 m) { return u32(rd<4>(a, m)); }
	u64 read_qword(offs_t a) { return rd<8>(a, ~u64(0)); }
	u16 read_word_unaligned(offs_t a) { return read_word(a); }
	u32 read_dword_unaligned(offs_t a) { return read_dword(a); }
	u32 read_dword_unaligned(offs_t a, u32 m) { return read_dword(a, m); }
	void write_byte(offs_t a, u8 d) { wr<1>(a, d, 0xff); }
	void write_word(offs_t a, u16 d) { wr<2>(a, d, 0xffff); }
	void write_word(offs_t a, u16 d, u16 m) { wr<2>(a, d, m); }
	void write_dword(offs_t a, u32 d) { wr<4>(a, d, 0xffffffff); }
	void write_dword(offs_t a, u32 d, u32 m) { wr<4>(a, d, m); }
	void write_qword(offs_t a, u64 d) { wr<8>(a, d, ~u64(0)); }
	void write_word_unaligned(offs_t a, u16 d) { write_word(a, d); }
	void write_dword_unaligned(offs_t a, u32 d) { write_dword(a, d); }
	void write_dword_unaligned(offs_t a, u32 d, u32 m) { write_dword(a, d, m); }
};

template <int AddrWidth, int Width, int AddrShift, endianness_t Endian> struct memory_access
{
	using specific = memory_access_specific<Width, AddrShift, Endian>;
	using cache = memory_access_specific<Width, AddrShift, Endian>;
};

class address_space
{
public:
	mem_bus *m_bus = nullptr;
	template <typename T> void cache(T &c) { c.bus = m_bus; }
	template <typename T> void specific(T &c) { c.bus = m_bus; }
	// convenience accessors used by some devices
	u8 read_byte(offs_t a) { return u8(m_bus->read(a, 0xff)); }
	u16 read_word(offs_t a) { return u16(m_bus->read(a, 0xffff)); }
	void write_byte(offs_t a, u8 d) { m_bus->write(a, d, 0xff); }
	void write_word(offs_t a, u16 d) { m_bus->write(a, d, 0xffff); }
	template <typename... A> void install_read_handler(A &&...) {}
	template <typename... A> void install_write_handler(A &&...) {}
	template <typename... A> void install_readwrite_handler(A &&...) {}
	template <typename... A> void install_ram(A &&...) {}
	template <typename... A> void unmap_readwrite(A &&...) {}
};

// address maps: accepted syntactically (internal maps), routing is done by the board.
struct address_map_entry
{
	template <typename... A> address_map_entry &rw(A &&...) { return *this; }
	template <typename... A> address_map_entry &r(A &&...) { return *this; }
	template <typename... A> address_map_entry &w(A &&...) { return *this; }
	template <typename... A> address_map_entry &ram(A &&...) { return *this; }
	template <typename... A> address_map_entry &rom(A &&...) { return *this; }
	template <typename... A> address_map_entry &mirror(A &&...) { return *this; }
	template <typename... A> address_map_entry &share(A &&...) { return *this; }
	template <typename... A> address_map_entry &region(A &&...) { return *this; }
	template <typename... A> address_map_entry &mask(A &&...) { return *this; }
	template <typename... A> address_map_entry &umask16(A &&...) { return *this; }
	template <typename... A> address_map_entry &umask32(A &&...) { return *this; }
	template <typename... A> address_map_entry &nopw(A &&...) { return *this; }
	template <typename... A> address_map_entry &nopr(A &&...) { return *this; }
	template <typename... A> address_map_entry &noprw(A &&...) { return *this; }
	template <typename... A> address_map_entry &unmaprw(A &&...) { return *this; }
	template <typename... A> address_map_entry &select(A &&...) { return *this; }
	template <typename... A> address_map_entry &lr16(A &&...) { return *this; }
	template <typename... A> address_map_entry &lw16(A &&...) { return *this; }
	template <typename... A> address_map_entry &lrw16(A &&...) { return *this; }
};
class address_map
{
public:
	address_map_entry &operator()(offs_t, offs_t) { return m_e; }
	void unmap_value_high() {}
	void unmap_value_low() {}
	void global_mask(offs_t) {}
private:
	address_map_entry m_e;
};
class address_map_constructor
{
public:
	address_map_constructor() = default;
	template <typename... A> address_map_constructor(A &&...) {}
};

class address_space_config
{
public:
	address_space_config() = default;
	address_space_config(const char *name, endianness_t e, u8 dw, u8 aw, s8 as = 0, address_map_constructor = address_map_constructor())
		: m_name(name), m_endian(e), m_data_width(dw), m_addr_width(aw), m_addr_shift(as) {}
	address_space_config(const char *name, endianness_t e, u8 dw, u8 aw, s8 as, u8 /*logaw*/, u8 /*pageshift*/, address_map_constructor = address_map_constructor())
		: m_name(name), m_endian(e), m_data_width(dw), m_addr_width(aw), m_addr_shift(as) {}
	const char *m_name = nullptr;
	endianness_t m_endian = ENDIANNESS_LITTLE;
	u8 m_data_width = 8, m_addr_width = 16;
	s8 m_addr_shift = 0;
};

// --------------------------------------------------------------------------
// screen / bitmaps
// --------------------------------------------------------------------------
struct rectangle
{
	int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
	rectangle() = default;
	rectangle(int x0, int x1, int y0, int y1) : min_x(x0), max_x(x1), min_y(y0), max_y(y1) {}
	int width() const { return max_x + 1 - min_x; }
	int height() const { return max_y + 1 - min_y; }
	void set(int x0, int x1, int y0, int y1) { min_x = x0; max_x = x1; min_y = y0; max_y = y1; }
};

template <typename T> class bitmap_t
{
public:
	void allocate(int w, int h) { m_w = w; m_h = h; m_data.assign(size_t(w) * h, 0); }
	T &pix(int y, int x = 0) { return m_data[size_t(y) * m_w + x]; }
	const T &pix(int y, int x = 0) const { return m_data[size_t(y) * m_w + x]; }
	int width() const { return m_w; }
	int height() const { return m_h; }
	int rowpixels() const { return m_w; }
	T *raw() { return m_data.data(); }
private:
	int m_w = 0, m_h = 0;
	std::vector<T> m_data;
};
using bitmap_ind16 = bitmap_t<u16>;
using bitmap_rgb32 = bitmap_t<u32>;

struct rgb_t
{
	static constexpr u32 black() { return 0xff000000; }
};

struct palette_stub { pen_t black_pen() const { return 0; } };

class screen_device
{
public:
	explicit screen_device(running_machine &m) : m_machine(m) {}
	// configuration
	void set_raw(u32 pixclock, int htotal, int hbend, int hbstart, int vtotal, int vbend, int vbstart);
	void configure(int width, int height, const rectangle &visarea, attotime frame_period);
	// state
	int width() const { return m_width; }
	int height() const { return m_height; }
	const rectangle &visible_area() const { return m_visarea; }
	attotime frame_period() const { return m_frame_period; }
	int vpos() const;
	int hpos() const;
	attotime time_until_pos(int vpos, int hpos = 0) const;
	attotime time_until_vblank_start() const { return time_until_pos(m_visarea.max_y + 1); }
	bool vblank() const { int v = vpos(); return v < m_visarea.min_y || v > m_visarea.max_y; }
	void update_partial(int scanline);
	void update_now() {}
	palette_stub &palette() { return m_pal; }
	u64 frame_number() const { return m_frame_number; }

	// called by the machine at the start of each frame
	void begin_frame();
	attotime frame_start() const { return m_frame_start; }

	// rendering: callback drawing one scanline into m_bitmap
	std::function<void(screen_device &, bitmap_ind16 &, const rectangle &)> m_update_ind16;
	bitmap_ind16 m_bitmap;
	int m_last_partial = -1;
	std::function<void()> on_vblank;   // frame complete (called at vblank start)

private:
	running_machine &m_machine;
	int m_width = 0, m_height = 0;
	rectangle m_visarea;
	attotime m_frame_period = attotime::from_hz(60);
	attoseconds_t m_scantime = 0, m_pixeltime = 0;
	attotime m_frame_start;
	u64 m_frame_number = 0;
	palette_stub m_pal;
	emu_timer *m_vbl_timer = nullptr;
	friend class running_machine;
};

// --------------------------------------------------------------------------
// devices
// --------------------------------------------------------------------------
struct device_state_entry
{
	device_state_entry &noshow() { return *this; }
	device_state_entry &readonly() { return *this; }
	device_state_entry &callimport() { return *this; }
	device_state_entry &callexport() { return *this; }
	template <typename T> device_state_entry &mask(T) { return *this; }
	device_state_entry &formatstr(const char *) { return *this; }
	template <typename T> device_state_entry &signed_mask(T) { return *this; }
	int index() const { return 0; }
};

class device_t
{
public:
	device_t(const machine_config &, device_type type, const char *tag, device_t *owner, u32 clock);
	virtual ~device_t() = default;

	running_machine &machine() const { return *m_machine; }
	const char *tag() const { return m_tag.c_str(); }
	const char *basetag() const { return m_tag.c_str(); }
	u32 clock() const { return m_clock; }
	void set_clock(u32 c) { m_clock = c; }
	void set_unscaled_clock(u32 c) { m_clock = c; }
	attotime clocks_to_attotime(u64 clocks) const { return attotime::from_ticks(clocks, m_clock); }
	u64 attotime_to_clocks(const attotime &t) const { return t.as_ticks(m_clock); }

	// lifecycle (called by the machine)
	void start() { device_start(); }
	void reset() { device_reset(); }

	template <typename T> emu_timer *timer_alloc(void (T::*fn)(s32), const char *, T *obj)
	{
		return timer_alloc_raw([obj, fn](s32 p) { (obj->*fn)(p); });
	}
	emu_timer *timer_alloc_raw(std::function<void(s32)> cb);

	template <typename... A> void save_item(A &&...) {}
	template <typename... A> void save_pointer(A &&...) {}

	template <typename... A> void logerror(const char *fmt, A &&... args) const
	{
		if (g_emu_log_enabled) emu_log_line(std::string(tag()) + ": " + string_format(fmt, std::forward<A>(args)...));
	}

	void attach(running_machine &m) { m_machine = &m; }

protected:
	virtual void device_start() {}
	virtual void device_reset() {}
	virtual void device_post_load() {}
	virtual void device_pre_save() {}
	virtual void device_clock_changed() {}

	running_machine *m_machine = nullptr;
	std::string m_tag;
	u32 m_clock = 0;
};

class device_video_interface
{
public:
	device_video_interface(const machine_config &, device_t &) {}
	void set_screen(screen_device &s) { m_screen = &s; }
	template <typename T> void set_screen(T &&) {}
	screen_device &screen() const { return *m_screen; }
	bool has_screen() const { return m_screen != nullptr; }
protected:
	screen_device *m_screen = nullptr;
};

struct device_memory_interface { using space_config_vector = std::vector<std::pair<int, const address_space_config *>>; };

class cpu_device : public device_t
{
public:
	using space_config_vector = std::vector<std::pair<int, const address_space_config *>>;

	cpu_device(const machine_config &mc, device_type type, const char *tag, device_t *owner, u32 clock)
		: device_t(mc, type, tag, owner, clock) {}

	// ---- execute interface (public part)
	void set_input_line(int line, int state);
	void set_input_line_and_vector(int line, int state, int) { set_input_line(line, state); }
	int input_line_state(int line) const { return (line >= 0 && line < 40) ? m_input_state[line] : 0; }
	void abort_timeslice();
	void eat_cycles(int cycles) { if (m_icountptr) { if (cycles > *m_icountptr) cycles = *m_icountptr; *m_icountptr -= cycles; } }
	void adjust_icount(int delta) { if (m_icountptr) *m_icountptr += delta; }
	u64 total_cycles() const;
	attotime local_time() const;
	bool suspended() const { return m_halted || m_in_reset; }
	void suspend_until_trigger(int, bool) {}
	void spin_until_trigger(int) {}
	void spin_until_interrupt() {}
	void spin_until_time(attotime) {}
	void spin() {}
	void signal_interrupt_trigger() { abort_timeslice(); }
	attotime cycles_to_attotime(u64 cycles) const { return clocks_to_attotime(execute_cycles_to_clocks(cycles)); }
	u64 attotime_to_cycles(const attotime &t) const { return execute_clocks_to_cycles(attotime_to_clocks(t)); }
	offs_t pc() const { return m_pc_hint ? m_pc_hint() : 0; }
	std::function<offs_t()> m_pc_hint;
	int cycles_remaining() const { return m_icountptr ? *m_icountptr : 0; }

	// irq acknowledge callback (board side)
	std::function<int(int)> m_irq_ack;

	// ---- memory interface
	address_space &space(int spacenum = AS_PROGRAM) { return m_spaces[spacenum]; }
	bool has_space(int spacenum) const { return m_spaces[spacenum].m_bus != nullptr; }
	void set_bus(int spacenum, mem_bus *bus) { m_spaces[spacenum].m_bus = bus; }
	template <typename... A> void set_addrmap(A &&...) {}
	bool has_configured_map(int) const { return false; }

	// ---- scheduler entry: run until 'target' time
	void run_until(const attotime &target);
	attotime m_localtime;      // time up to which this CPU has executed
	bool m_executing_now = false;
	attotime m_slice_start;
	int m_slice_cycles = 0;
	u64 m_total_cycles_base = 0;

	virtual u64 execute_clocks_to_cycles(u64 clocks) const noexcept { return clocks; }
	virtual u64 execute_cycles_to_clocks(u64 cycles) const noexcept { return cycles; }

protected:
	virtual u32 execute_min_cycles() const noexcept { return 1; }
	virtual u32 execute_max_cycles() const noexcept { return 1; }
	virtual u32 execute_input_lines() const noexcept { return 1; }
	virtual bool execute_input_edge_triggered(int) const noexcept { return false; }
	virtual void execute_run() = 0;
	virtual void execute_set_input(int, int) {}
	virtual space_config_vector memory_space_config() const { return {}; }
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() { return nullptr; }
	virtual void state_import(const device_state_entry &) {}
	virtual void state_export(const device_state_entry &) {}
	virtual void state_string_export(const device_state_entry &, std::string &) const {}

	void set_icountptr(int &icount) { m_icountptr = &icount; }
	int standard_irq_callback(int line, offs_t pc = 0);
	template <typename T> device_state_entry &state_add(int, const char *, T &) { return m_dummy_state; }
	template <typename T> device_state_entry &state_add(int, const std::string &, T &) { return m_dummy_state; }
	device_state_entry &state_add(int, const char *, u32) { return m_dummy_state; }
	template <typename T, typename G, typename S> device_state_entry &state_add(int, const char *, G &&, S &&) { return m_dummy_state; }
	void debugger_instruction_hook(offs_t pc) { if (m_trace && !m_trace_stop && !(pc >= m_skip_lo && pc <= m_skip_hi) && !(pc >= m_skip2_lo && pc <= m_skip2_hi)) { m_trace[m_trace_pos++ & 4095] = pc; if (pc < m_trace_lo || m_trace_pos >= m_trace_max) m_trace_stop = true; } }
	void debugger_wait_hook() {}
	void debugger_exception_hook(int) {}
	bool debugger_enabled() const { return m_trace != nullptr; }
	bool machine_debug_flags() const { return false; }

	int *m_icountptr = nullptr;
public:
	offs_t *m_trace = nullptr;
	u32 m_trace_pos = 0; bool m_trace_stop = false; offs_t m_trace_lo = 0; offs_t m_skip_lo = 1, m_skip_hi = 0, m_skip2_lo = 1, m_skip2_hi = 0; u32 m_trace_max = 0xffffffff;
protected:
	int m_input_state[40] = {};
	bool m_halted = false;
	bool m_in_reset = false;
	address_space m_spaces[4];
	device_state_entry m_dummy_state;
};

// --------------------------------------------------------------------------
// running machine
// --------------------------------------------------------------------------
enum { KEYCODE_L = 0 };
struct input_manager_stub { bool code_pressed(int) const { return false; } };
class running_machine
{
public:
	running_machine();
	input_manager_stub &input() { static input_manager_stub s; return s; }
	void debug_break() {}
	~running_machine();

	device_scheduler &scheduler() { return m_scheduler; }
	attotime time() const;
	std::string describe_context() const;
	bool side_effects_disabled() const { return false; }
	u32 rand() { m_rand = m_rand * 1103515245u + 12345u; return m_rand >> 8; }

	// registration
	void add_cpu(cpu_device &cpu) { m_cpus.push_back(&cpu); cpu.attach(*this); }
	void add_device(device_t &d) { d.attach(*this); }
	screen_device &screen() { return *m_screen; }

	// timers
	emu_timer *timer_alloc(std::function<void(s32)> cb);
	void timer_scheduled(emu_timer *t);
	void timer_set(attotime duration, std::function<void(s32)> cb, s32 param);

	// run the system until the given absolute time
	void run_until(const attotime &target);
	attotime m_basetime;           // global time
	cpu_device *m_executing = nullptr;
	attotime m_max_slice = attotime::from_usec(50);
	bool m_abort_slice = false;

private:
	device_scheduler m_scheduler;
	std::vector<cpu_device *> m_cpus;
	std::vector<std::unique_ptr<emu_timer>> m_timers;
	std::vector<std::unique_ptr<emu_timer>> m_oneshots;
	std::unique_ptr<screen_device> m_screen;
	u32 m_rand = 0x9876;
	u64 m_timer_seq = 0;
	emu_timer *next_timer();
};

#endif // __EMU_H__
