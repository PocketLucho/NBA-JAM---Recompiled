// ui.cpp - see ui.h
#include "ui.h"
#include <cmath>
#include <cstdio>

namespace ui
{

// ---------------------------------------------------------------- input
const char *act_name(act a)
{
	static const char *n[] = { "none", "up", "down", "left", "right", "confirm", "back", "tab_prev", "tab_next", "page_up", "page_down", "context" };
	return n[int(a)];
}

static bool repeats(act a) { return a == act::up || a == act::down || a == act::left || a == act::right || a == act::page_up || a == act::page_down; }

void input::hold(act a, unsigned now)
{
	push(a);
	m_held = a;
	m_next = now + kRepeatDelay;
	m_interval = kRepeatStart;
}

void input::release(act a) { if (m_held == a) m_held = act::none; }

void input::tick(unsigned now)
{
	if (m_held == act::none || now < m_next) return;
	push(m_held);
	m_interval = std::max(kRepeatMin, m_interval * 85 / 100);   // faster the longer it is held
	m_next = now + m_interval;
}

bool input::next(event &out)
{
	if (m_q.empty()) return false;
	out = m_q.front();
	m_q.pop_front();
	return true;
}

static act key_act(key k, bool shift)
{
	switch (k)
	{
	case key::up: return act::up;
	case key::down: return act::down;
	case key::left: return act::left;
	case key::right: return act::right;
	case key::enter: case key::space: return act::confirm;
	case key::escape: case key::backspace: return act::back;
	case key::tab: return shift ? act::tab_prev : act::tab_next;
	case key::q: return act::tab_prev;
	case key::e: return act::tab_next;
	case key::page_up: return act::page_up;
	case key::page_down: return act::page_down;
	case key::del: return act::context;
	default: return act::none;
	}
}

static act pad_act(pad p)
{
	switch (p)
	{
	case pad::up: return act::up;
	case pad::down: return act::down;
	case pad::left: return act::left;
	case pad::right: return act::right;
	case pad::south: case pad::start: return act::confirm;
	case pad::east: return act::back;
	case pad::west: return act::context;
	case pad::l1: return act::tab_prev;
	case pad::r1: return act::tab_next;
	case pad::l2: return act::page_up;
	case pad::r2: return act::page_down;
	default: return act::none;
	}
}

void input::feed(const raw_event &e, unsigned now)
{
	auto down = [&](act a) { if (a == act::none) return; if (repeats(a)) hold(a, now); else push(a); };
	switch (e.kind)
	{
	case raw_event::key_down: m_dev = device::keyboard; down(key_act(e.k, e.shift)); break;
	case raw_event::key_up: release(key_act(e.k, false)); release(key_act(e.k, true)); break;
	case raw_event::pad_down: m_dev = e.dev; down(pad_act(e.p)); break;
	case raw_event::pad_up: release(pad_act(e.p)); break;
	case raw_event::pad_stick:
	{
		// dead zone with hysteresis, so a stick resting near the edge of it does not flutter
		auto step = [](int was, float v) { return v > 0.6f ? 1 : v < -0.6f ? -1 : std::fabs(v) < 0.4f ? 0 : was; };
		int nx = step(m_stick_x, e.x), ny = step(m_stick_y, e.y);
		if (nx != m_stick_x)
		{
			release(m_stick_x < 0 ? act::left : act::right);
			if (nx) { m_dev = e.dev; hold(nx < 0 ? act::left : act::right, now); }
			m_stick_x = nx;
		}
		if (ny != m_stick_y)
		{
			release(m_stick_y < 0 ? act::up : act::down);
			if (ny) { m_dev = e.dev; hold(ny < 0 ? act::up : act::down, now); }
			m_stick_y = ny;
		}
		break;
	}
	case raw_event::mouse_move:
	{
		m_dev = device::mouse;
		event o; o.ptr = event::hover; o.x = e.x; o.y = e.y;
		m_q.push_back(o);
		break;
	}
	case raw_event::mouse_down: m_dev = device::mouse; m_mouse_down = true; m_down_x = e.x; m_down_y = e.y; break;
	case raw_event::mouse_up:
		if (m_mouse_down && std::fabs(e.x - m_down_x) < 6 && std::fabs(e.y - m_down_y) < 6)
		{
			event o; o.ptr = event::click; o.x = e.x; o.y = e.y;
			m_q.push_back(o);
		}
		m_mouse_down = false;
		break;
	case raw_event::wheel:
	{
		m_dev = device::mouse;
		event o; o.ptr = event::scroll; o.x = e.x; o.y = 0; o.amount = -e.y * 3.0f;   // wheel towards you = further down
		o.y = m_last_y;
		m_q.push_back(o);
		break;
	}
	case raw_event::touch_down: m_dev = device::touch; m_touch = true; m_dragged = false; m_down_x = e.x; m_down_y = m_last_y = e.y; break;
	case raw_event::touch_move:
		if (!m_touch) break;
		if (std::fabs(e.y - m_down_y) > 6) m_dragged = true;
		if (m_dragged)
		{
			// the list follows the finger: one row for every row-height dragged
			float rows = (m_last_y - e.y) / float(screen::kRow);
			if (std::fabs(rows) >= 1.0f)
			{
				event o; o.ptr = event::scroll; o.x = m_down_x; o.y = m_down_y; o.amount = float(int(rows));
				m_q.push_back(o);
				m_last_y -= float(int(rows)) * screen::kRow;
			}
		}
		break;
	case raw_event::touch_up:
		if (m_touch && !m_dragged)
		{
			event o; o.ptr = event::click; o.x = e.x; o.y = e.y;
			m_q.push_back(o);
		}
		m_touch = false;
		break;
	}
	if (e.kind == raw_event::mouse_move) m_last_y = e.y;
}

std::string glyph(device d, act a)
{
	struct row { act a; const char *kb, *xb, *ps, *nin; };
	static const row t[] = {
		{ act::confirm, "Enter", "A", "Cross", "B" },          // the bottom button, whatever it is called
		{ act::back, "Esc", "B", "Circle", "A" },
		{ act::context, "Del", "X", "Square", "Y" },
		{ act::tab_prev, "Q", "LB", "L1", "L" },
		{ act::tab_next, "E", "RB", "R1", "R" },
		{ act::page_up, "PgUp", "LT", "L2", "ZL" },
		{ act::page_down, "PgDn", "RT", "R2", "ZR" },
		{ act::up, "Up", "Up", "Up", "Up" }, { act::down, "Down", "Down", "Down", "Down" },
		{ act::left, "Left", "Left", "Left", "Left" }, { act::right, "Right", "Right", "Right", "Right" },
	};
	for (const row &r : t)
		if (r.a == a)
			switch (d)
			{
			case device::xbox: return r.xb;
			case device::playstation: return r.ps;
			case device::nintendo: return r.nin;
			case device::touch: return a == act::confirm ? "Tap" : r.kb;
			case device::mouse: return a == act::confirm ? "Click" : r.kb;
			default: return r.kb;
			}
	return "";
}

// ---------------------------------------------------------------- text
std::string fit(const std::string &s, int max_chars)
{
	if (max_chars <= 0) return "";
	if (int(s.size()) <= max_chars) return s;
	if (max_chars <= 2) return s.substr(0, size_t(max_chars));
	std::string t = s.substr(0, size_t(max_chars - 2));
	while (!t.empty() && t.back() == ' ') t.pop_back();
	return t + "..";
}

std::string marquee(const std::string &s, int max_chars, unsigned now_ms)
{
	if (max_chars <= 0) return "";
	if (int(s.size()) <= max_chars) return s;
	// pause, slide to the end at 6 characters a second, pause, start again
	const unsigned over = unsigned(s.size()) - unsigned(max_chars), pause = 900, per_char = 170;
	unsigned cycle = pause * 2 + over * per_char, t = now_ms % cycle;
	unsigned off = t < pause ? 0 : std::min(over, (t - pause) / per_char);
	return s.substr(off, size_t(max_chars));
}

// ---------------------------------------------------------------- items
item header(const std::string &l) { item i; i.k = kind::header; i.label = l; return i; }
item label(const std::string &l) { item i; i.k = kind::label; i.label = l; return i; }
item button(int id, const std::string &l, const std::string &v) { item i; i.k = kind::button; i.id = id; i.label = l; i.value_text = v; return i; }
item toggle(int id, const std::string &l, bool on) { item i; i.k = kind::toggle; i.id = id; i.label = l; i.on = on; return i; }
item picker(int id, const std::string &l, const std::vector<std::string> &c, int choice)
{
	item i; i.k = kind::picker; i.id = id; i.label = l; i.choices = c;
	i.choice = c.empty() ? 0 : std::clamp(choice, 0, int(c.size()) - 1);
	return i;
}
item slider(int id, const std::string &l, float v, float mn, float mx, float st)
{
	item i; i.k = kind::slider; i.id = id; i.label = l; i.min = mn; i.max = mx; i.step = st; i.value = std::clamp(v, mn, mx);
	return i;
}

// ---------------------------------------------------------------- layout
static const float kX0 = 8, kW0 = screen::kW - 16;
static int strip_h(const panel &p) { return p.title.empty() ? 2 : 13; }

rectf screen::panel_rect(int p) const
{
	const panel &pn = panels()[size_t(p)];
	const float h0 = float(kBottom - kTop);
	return { std::floor(kX0 + pn.r.x * kW0), std::floor(kTop + pn.r.y * h0), std::floor(pn.r.w * kW0), std::floor(pn.r.h * h0) };
}

int screen::rows_visible(int p) const
{
	const panel &pn = panels()[size_t(p)];
	return std::max(1, int(panel_rect(p).h - strip_h(pn) - 3) / kRow);
}

rectf screen::tab_rect(int t) const
{
	float x = kX0;
	for (int i = 0; i < t; i++) x += float(text_w(tabs[size_t(i)].name, face::list)) + 14;
	return { x, float(kTabsY), float(text_w(tabs[size_t(t)].name, face::list)) + 10, 13 };
}

std::vector<screen::slot> screen::slots() const
{
	std::vector<slot> out;
	if (tabs.empty()) return out;
	for (int p = 0; p < int(panels().size()); p++)
	{
		const panel &pn = panels()[size_t(p)];
		rectf r = panel_rect(p);
		int rows = rows_visible(p), cols = std::max(1, pn.cols);
		float cw = (r.w - 6) / cols;
		for (int i = 0; i < int(pn.items.size()); i++)
		{
			int row = i / cols, col = i % cols;
			if (row < pn.scroll || row >= pn.scroll + rows) continue;
			out.push_back({ p, i, { r.x + 3 + col * cw, r.y + strip_h(pn) + 1 + float(row - pn.scroll) * kRow, cw, float(kRow) } });
		}
	}
	return out;
}

// A panel of text only (nothing to select) is read by scrolling it with the directions.
bool screen::scroll_text(int by)
{
	panel &p = *cur();
	for (const auto &i : p.items) if (i.focusable()) return false;
	int cols = std::max(1, p.cols), total = (int(p.items.size()) + cols - 1) / cols;
	int ns = std::clamp(p.scroll + by, 0, std::max(0, total - rows_visible(cur_panel)));
	if (ns == p.scroll) return false;
	p.scroll = ns;
	return true;
}

void screen::keep_visible(panel &p) const
{
	int idx = int(&p - &panels()[0]);
	int rows = rows_visible(idx), cols = std::max(1, p.cols), row = p.focus / cols;
	int total = (int(p.items.size()) + cols - 1) / cols;
	// a header just above the focused item stays in view with it
	int want_top = row;
	if (cols == 1 && row > 0 && p.items[size_t(row - 1)].k == kind::header) want_top = row - 1;
	if (want_top < p.scroll) p.scroll = want_top;
	if (row >= p.scroll + rows) p.scroll = row - rows + 1;
	p.scroll = std::clamp(p.scroll, 0, std::max(0, total - rows));
}

// ---------------------------------------------------------------- focus
item *screen::focused()
{
	if (tabs.empty() || panels().empty()) return nullptr;
	panel &p = panels()[size_t(cur_panel)];
	if (p.focus < 0 || p.focus >= int(p.items.size())) return nullptr;
	return &p.items[size_t(p.focus)];
}

item *screen::find(int id)
{
	for (auto &t : tabs) for (auto &p : t.panels) for (auto &i : p.items) if (i.id == id && i.k != kind::header && i.k != kind::label) return &i;
	return nullptr;
}

bool screen::focus_ok() const
{
	if (tabs.empty()) return true;
	bool any = false;
	for (const auto &p : panels()) for (const auto &i : p.items) any = any || i.focusable();
	const item *f = focused();
	return any ? (f && f->focusable()) : true;
}

void screen::fix_focus()
{
	if (tabs.empty()) return;
	cur_tab = std::clamp(cur_tab, 0, int(tabs.size()) - 1);
	if (panels().empty()) { cur_panel = 0; return; }
	cur_panel = std::clamp(cur_panel, 0, int(panels().size()) - 1);
	// keep the focused item if it is usable; otherwise the nearest usable one in its panel,
	// then the first usable one in any panel
	auto usable = [](const panel &p, int i) { return i >= 0 && i < int(p.items.size()) && p.items[size_t(i)].focusable(); };
	for (int k = 0; k < int(panels().size()); k++)
	{
		int pi = (cur_panel + k) % int(panels().size());
		panel &p = panels()[size_t(pi)];
		if (usable(p, p.focus)) { cur_panel = pi; keep_visible(p); return; }
		for (int d = 1; d < int(p.items.size()) + 1; d++)
			for (int s : { p.focus + d, p.focus - d })
				if (usable(p, s)) { p.focus = s; cur_panel = pi; keep_visible(p); return; }
	}
}

void screen::set_tab(int t)
{
	int n = int(tabs.size());
	cur_tab = ((t % n) + n) % n;
	cur_panel = 0;
	fix_focus();
}

bool screen::move_in_panel(int step)
{
	panel &p = *cur();
	int cols = std::max(1, p.cols), n = int(p.items.size());
	bool moved = false;
	if (cols > 1 && (step == 1 || step == -1))
	{
		// along a grid row
		int i = p.focus + step;
		while (i >= 0 && i < n && i / cols == p.focus / cols && !p.items[size_t(i)].focusable()) i += step;
		if (i < 0 || i >= n || i / cols != p.focus / cols) return false;
		p.focus = i;
		keep_visible(p);
		return true;
	}
	int dir = step > 0 ? 1 : -1, left = std::abs(step) / (cols > 1 ? cols : 1);
	if (cols > 1 && std::abs(step) < cols) left = 1;
	int unit = cols > 1 ? cols * dir : dir;
	for (int i = p.focus + unit; i >= 0 && i < n && left > 0; i += unit)
		if (p.items[size_t(i)].focusable()) { p.focus = i; moved = true; left--; }
	if (moved) keep_visible(p);
	return moved;
}

bool screen::move_to_panel(int dx, int dy)
{
	rectf from = panel_rect(cur_panel);
	float fy = from.y + from.h / 2;
	for (const slot &s : slots()) if (s.panel == cur_panel && s.index == cur()->focus) fy = s.r.y + s.r.h / 2;
	int best = -1;
	float best_d = 1e9f;
	for (int i = 0; i < int(panels().size()); i++)
	{
		if (i == cur_panel) continue;
		bool any = false;
		for (const auto &it : panels()[size_t(i)].items) any = any || it.focusable();
		if (!any) continue;
		rectf r = panel_rect(i);
		float d;
		if (dx)
		{
			// to that side, and sharing some height with this panel
			bool side = dx > 0 ? r.x >= from.x + from.w - 1 : r.x + r.w <= from.x + 1;
			if (!side || r.y >= from.y + from.h || r.y + r.h <= from.y) continue;
			d = std::fabs(r.x - from.x) + (fy >= r.y && fy < r.y + r.h ? 0.0f : 500.0f);
		}
		else
		{
			bool side = dy > 0 ? r.y >= from.y + from.h - 1 : r.y + r.h <= from.y + 1;
			if (!side || r.x >= from.x + from.w || r.x + r.w <= from.x) continue;
			d = std::fabs(r.y - from.y) + std::fabs(r.x - from.x) * 0.01f;
		}
		if (d < best_d) { best_d = d; best = i; }
	}
	if (best < 0) return false;
	panel &to = panels()[size_t(best)];
	int cols = std::max(1, to.cols), pick = -1;
	if (dy)
	{
		// coming from above: the first usable item; from below: the last
		for (int i = 0; i < int(to.items.size()); i++) if (to.items[size_t(i)].focusable()) { pick = i; if (dy > 0) break; }
	}
	else
	{
		// the usable item nearest in height to where focus was
		rectf r = panel_rect(best);
		float bd = 1e9f;
		for (int i = 0; i < int(to.items.size()); i++)
		{
			if (!to.items[size_t(i)].focusable()) continue;
			float y = r.y + strip_h(to) + 1 + float(i / cols - to.scroll) * kRow + kRow / 2.0f, d = std::fabs(y - fy);
			if (d < bd) { bd = d; pick = i; }
		}
	}
	if (pick < 0) return false;
	cur_panel = best;
	to.focus = pick;
	keep_visible(to);
	return true;
}

// ---------------------------------------------------------------- acting on items
result screen::change(item &it, int dir)
{
	result r;
	switch (it.k)
	{
	case kind::toggle: it.on = !it.on; r.what = result::changed; break;
	case kind::picker:
		if (it.choices.size() > 1)
		{
			int n = int(it.choices.size());
			it.choice = ((it.choice + dir) % n + n) % n;
			r.what = result::changed;
		}
		break;
	case kind::slider:
	{
		float v = std::clamp(it.value + dir * it.step, it.min, it.max);
		// keep the value on the step grid, so 0.1 steps do not drift to 0.30000001
		v = it.min + std::round((v - it.min) / it.step) * it.step;
		v = std::clamp(v, it.min, it.max);
		if (v != it.value) { it.value = v; r.what = result::changed; }
		break;
	}
	default: break;
	}
	r.id = it.id;
	return r;
}

result screen::press(item &it)
{
	result r;
	r.id = it.id;
	if (it.k == kind::button) r.what = result::activated;
	else if (it.k == kind::toggle || it.k == kind::picker) return change(it, 1);
	return r;
}

static rectf dialog_button(bool yes) { return { yes ? 120.0f : 216.0f, 164, 64, 16 }; }

result screen::handle(const event &e)
{
	result r;
	if (tabs.empty()) { if (e.a == act::back) r.what = result::back; return r; }
	if (asking())
	{
		// a dialog takes everything: Left / Right choose, Confirm answers, Back means No
		bool answer = false, yes = false;
		if (e.a == act::left || e.a == act::right) m_dialog_yes = !m_dialog_yes;
		else if (e.a == act::confirm) { answer = true; yes = m_dialog_yes; }
		else if (e.a == act::back) { answer = true; yes = false; }
		else if (e.ptr == event::hover) { if (dialog_button(true).has(e.x, e.y)) m_dialog_yes = true; if (dialog_button(false).has(e.x, e.y)) m_dialog_yes = false; }
		else if (e.ptr == event::click)
		{
			if (dialog_button(true).has(e.x, e.y)) { answer = true; yes = true; }
			if (dialog_button(false).has(e.x, e.y)) { answer = true; yes = false; }
		}
		if (answer) { r.what = yes ? result::yes : result::no; r.id = m_dialog_id; m_dialog.clear(); }
		return r;
	}
	if (e.ptr != event::p_none)
	{
		if (e.ptr == event::click)
			for (int t = 0; t < int(tabs.size()); t++) if (tab_rect(t).has(e.x, e.y)) { set_tab(t); return r; }
		if (e.ptr == event::scroll)
		{
			int target = cur_panel;
			for (int p = 0; p < int(panels().size()); p++) if (panel_rect(p).has(e.x, e.y)) target = p;
			panel &p = panels()[size_t(target)];
			int cols = std::max(1, p.cols), rows = rows_visible(target), total = (int(p.items.size()) + cols - 1) / cols;
			int ns = std::clamp(p.scroll + int(e.amount), 0, std::max(0, total - rows));
			if (ns == p.scroll) return r;
			// focus follows the view if it would be left outside, but never onto a header
			int f = p.focus;
			bool text_only = true;
			for (const auto &i : p.items) text_only = text_only && !i.focusable();
			if (text_only) { p.scroll = ns; return r; }
			if (f / cols < ns || f / cols >= ns + rows)
			{
				f = -1;
				for (int i = ns * cols; i < std::min(int(p.items.size()), (ns + rows) * cols); i++)
					if (p.items[size_t(i)].focusable()) { f = i; if (e.amount > 0) break; }
			}
			if (f >= 0) { p.scroll = ns; p.focus = f; }
			return r;
		}
		for (const slot &s : slots())
		{
			if (!s.r.has(e.x, e.y)) continue;
			item &it = panels()[size_t(s.panel)].items[size_t(s.index)];
			if (!it.focusable()) return r;
			cur_panel = s.panel;
			panels()[size_t(s.panel)].focus = s.index;
			if (e.ptr == event::click)
			{
				if (it.k == kind::slider)
				{
					// the bar is the right 40% of the row
					float x0 = s.r.x + s.r.w * 0.55f, x1 = s.r.x + s.r.w - 34;
					float f = std::clamp((e.x - x0) / std::max(1.0f, x1 - x0), 0.0f, 1.0f);
					float v = it.min + std::round(f * (it.max - it.min) / it.step) * it.step;
					v = std::clamp(v, it.min, it.max);
					if (v != it.value) { it.value = v; r.what = result::changed; r.id = it.id; }
					return r;
				}
				if (it.k == kind::picker && e.x < s.r.x + s.r.w * 0.75f && e.x > s.r.x + s.r.w * 0.5f) return change(it, -1);   // the "<" side
				return press(it);
			}
			return r;
		}
		return r;
	}
	item *it = focused();
	switch (e.a)
	{
	case act::back: r.what = result::back; break;
	case act::tab_prev: set_tab(cur_tab - 1); break;
	case act::tab_next: set_tab(cur_tab + 1); break;
	case act::up: if (cur() && !move_in_panel(cur()->cols > 1 ? -cur()->cols : -1) && !scroll_text(-1)) move_to_panel(0, -1); break;
	case act::down: if (cur() && !move_in_panel(cur()->cols > 1 ? cur()->cols : 1) && !scroll_text(1)) move_to_panel(0, 1); break;
	case act::left:
	case act::right:
	{
		int d = e.a == act::left ? -1 : 1;
		if (it && it->focusable() && (it->k == kind::toggle || it->k == kind::picker || it->k == kind::slider)) return change(*it, d);
		if (cur() && !(cur()->cols > 1 && move_in_panel(d))) move_to_panel(d, 0);
		break;
	}
	case act::page_up: if (cur() && !move_in_panel(-rows_visible(cur_panel) * std::max(1, cur()->cols))) scroll_text(-(rows_visible(cur_panel) - 1)); break;
	case act::page_down: if (cur() && !move_in_panel(rows_visible(cur_panel) * std::max(1, cur()->cols))) scroll_text(rows_visible(cur_panel) - 1); break;
	case act::confirm: if (it && it->focusable()) return press(*it); break;
	case act::context: if (it && it->focusable()) { r.what = result::context; r.id = it->id; } break;
	default: break;
	}
	return r;
}

// ---------------------------------------------------------------- drawing
void screen::draw(canvas &cv, const theme &th, device dev, unsigned now_ms) const
{
	// all text is the project lettering (U2.1), measured in pixels
	// rows and tabs are in the theme's ROM font when it has one (U2.3), tinted by their colour
	const rom_art::font *lf = th.list_font.get();
	auto put = [&](int x, int y, const std::string &s, uint32_t c, face f = face::list)
	{
		if (lf && f == face::list) draw_text(cv, x, y + std::max(0, 8 - lf->cap_h) / 2, s, *lf, c);
		else draw_text(cv, x, y, s, f, th.ink_for(c));
	};
	const measure_fn wide = [lf](const std::string &s) { return lf ? text_w(s, *lf) : text_w(s, face::list); };

	// background: vertical gradient
	if (th.backdrop.size() == cv.px.size()) cv.px = th.backdrop;
	else
		for (int y = 0; y < cv.h; y++)
		{
			auto mix = [&](int sh) { int a = (th.bg_top >> sh) & 0xff, b = (th.bg_bottom >> sh) & 0xff; return uint32_t(a + (b - a) * y / std::max(1, cv.h - 1)) << sh; };
			cv.fill(0, y, cv.w, 1, 0xff000000 | mix(16) | mix(8) | mix(0));
		}
	if (const rom_art::font *tf = th.title_font.get())
	{
		std::string ttl = fit_px(title, cv.w - 16, [tf](const std::string &s) { return text_w(s, *tf); });
		draw_text(cv, cv.w / 2 - text_w(ttl, *tf) / 2, 3 + std::max(0, 16 - tf->cap_h) / 2, ttl, *tf);
	}
	else
	{
		std::string ttl = fit_px(title, cv.w - 16, face::title);
		draw_text(cv, cv.w / 2 - text_w(ttl, face::title) / 2, 3, ttl, face::title, { th.title_top, th.title_bottom, th.outline });
	}
	if (tabs.empty()) return;

	// tab bar
	for (int t = 0; t < int(tabs.size()); t++)
	{
		rectf r = tab_rect(t);
		bool sel = t == cur_tab;
		if (sel) cv.fill(int(r.x), int(r.y), int(r.w), int(r.h), th.strip);
		put(int(r.x) + 5, int(r.y) + 2, fit_px(tabs[size_t(t)].name, int(r.w) - 6, wide), sel ? th.tab_sel : th.tab);
		if (sel) cv.fill(int(r.x), int(r.y + r.h) - 2, int(r.w), 2, th.bar);
	}
	cv.fill(int(kX0), kTabsY + 13, int(kW0), 1, th.panel_edge);

	// panels
	for (int p = 0; p < int(panels().size()); p++)
	{
		const panel &pn = panels()[size_t(p)];
		rectf r = panel_rect(p);
		if (th.panel_alpha >= 255 || th.backdrop.empty()) cv.fill(int(r.x), int(r.y), int(r.w), int(r.h), th.panel);
		else cv.blend(int(r.x), int(r.y), int(r.w), int(r.h), th.panel, std::max(0, th.panel_alpha));
		cv.frame(int(r.x), int(r.y), int(r.w), int(r.h), p == cur_panel ? th.bar : th.panel_edge);
		if (!pn.title.empty())
		{
			cv.fill(int(r.x) + 1, int(r.y) + 1, int(r.w) - 2, 11, th.strip);
			put(int(r.x) + 4, int(r.y) + 3, fit_px(pn.title, int(r.w) - 8, face::note), th.strip_text, face::note);
		}
		int cols = std::max(1, pn.cols), rows = rows_visible(p), total = (int(pn.items.size()) + cols - 1) / cols;
		if (pn.scroll > 0) put(int(r.x + r.w) - 11, int(r.y) + strip_h(pn) + 2, "\x01", th.tab_sel);
		if (pn.scroll + rows < total) put(int(r.x + r.w) - 11, int(r.y + r.h) - 11, "\x02", th.tab_sel);
	}
	for (const slot &s : slots())
	{
		const panel &pn = panels()[size_t(s.panel)];
		const item &it = pn.items[size_t(s.index)];
		bool foc = s.panel == cur_panel && s.index == pn.focus && it.focusable() && !asking();
		int x = int(s.r.x), y = int(s.r.y), w = int(s.r.w) - (pn.cols > 1 ? 2 : 10);
		if (foc) cv.blend(x, y, w, kRow - 1, th.bar, 110);
		uint32_t col = it.disabled || (it.dim && !foc) ? th.text_off : it.k == kind::header ? th.header : foc ? th.text_focus : th.text;
		const int room = w - 8;
		if (pn.cols > 1)
		{
			// grid cell: centred, boxed
			cv.frame(x, y, w, kRow - 1, it.disabled ? th.text_off : th.panel_edge);
			std::string t = foc ? marquee_px(it.label, room, wide, now_ms) : fit_px(it.label, room, wide);
			put(x + (w - wide(t)) / 2, y + 2, t, col);
			continue;
		}
		// the value sits at the right; the label gets what is left
		std::string val;
		if (it.k == kind::toggle) val = it.on ? "ON" : "OFF";
		else if (it.k == kind::picker) val = "< " + (it.choices.empty() ? std::string("-") : it.choices[size_t(it.choice)]) + " >";
		else if (it.k == kind::slider) { char b[16]; snprintf(b, sizeof(b), it.step >= 1 ? "%.0f" : "%.1f", double(it.value)); val = b; }
		else if (it.k == kind::button || it.k == kind::label) val = it.value_text;
		int bar_w = it.k == kind::slider ? int(s.r.w * 0.45f) - 34 : 0;
		// a line of text gives its value all the room its name leaves
		std::string v = fit_px(val, it.k == kind::slider ? 30 : it.k == kind::label ? std::max(room / 2, room - wide(it.label) - 12) : room / 2, wide);
		int label_px = room - (v.empty() ? 0 : wide(v) + 8);
		if (bar_w > 8) label_px = w - 4 - 32 - bar_w - 12;      // up to the slider's bar, with a gap
		std::string t = foc ? marquee_px(it.label, label_px, wide, now_ms) : fit_px(it.label, label_px, wide);
		put(x + (it.k == kind::header ? 2 : 6), y + 2, t, col);
		if (!v.empty())
		{
			uint32_t vc = it.disabled ? th.text_off : it.k == kind::toggle ? (it.on ? th.header : th.text_off) : th.value;
			put(x + w - 4 - wide(v), y + 2, v, vc);
		}
		if (bar_w > 8)
		{
			int bx = x + w - 4 - 32 - bar_w, by = y + 4;
			cv.fill(bx, by, bar_w, 3, th.panel_edge);
			float f = it.max > it.min ? (it.value - it.min) / (it.max - it.min) : 0.0f;
			cv.fill(bx, by, int(bar_w * f), 3, it.disabled ? th.text_off : th.bar);
			cv.fill(bx + int((bar_w - 2) * f), by - 2, 2, 7, it.disabled ? th.text_off : th.tab_sel);
		}
	}

	// status line: what each button does here, in the words of the device used last
	std::string st = "[" + glyph(dev, act::confirm) + "] Select  [" + glyph(dev, act::back) + "] Back";
	if (tabs.size() > 1) st += "  [" + glyph(dev, act::tab_prev) + "/" + glyph(dev, act::tab_next) + "] Tab";
	for (const prompt &pr : prompts) st += "  [" + glyph(dev, pr.a) + "] " + pr.text;
	const int line = cv.w - 16;
	if (text_w(st, face::note) <= line) put(8, cv.h - 14, st, th.status, face::note);
	else
	{
		// two lines, broken between prompts
		size_t cut = std::string::npos;
		for (size_t at = st.find("  ["); at != std::string::npos; at = st.find("  [", at + 1))
			if (text_w(st.substr(0, at), face::note) <= line) cut = at;
		if (cut == std::string::npos) cut = st.size() / 2;
		put(8, cv.h - 24, fit_px(st.substr(0, cut), line, face::note), th.status, face::note);
		put(8, cv.h - 13, fit_px(st.substr(std::min(st.size(), cut + 2)), line, face::note), th.status, face::note);
	}

	if (!m_toast.empty() && now_ms < m_toast_until)
	{
		std::string t = fit_px(m_toast, line - 16, wide);
		int w = wide(t) + 16, x = (cv.w - w) / 2, y = kBottom - 18;
		cv.fill(x, y, w, 14, th.strip);
		cv.frame(x, y, w, 14, th.bar);
		put(x + 8, y + 3, t, th.tab_sel);
	}

	if (asking())
	{
		cv.blend(0, 0, cv.w, cv.h, 0xff000000, 150);
		int x = 70, y = 104, w = 260, h = 86;
		cv.fill(x, y, w, h, th.dialog);
		cv.frame(x, y, w, h, th.bar);
		// the question, wrapped inside the box
		int ty = y + 10;
		for (const std::string &l : wrap_px(m_dialog, w - 20, wide, 4)) { put(x + 10, ty, l, th.tab_sel); ty += 12; }
		for (bool yes : { true, false })
		{
			rectf b = dialog_button(yes);
			bool sel = yes == m_dialog_yes;
			cv.fill(int(b.x), int(b.y), int(b.w), int(b.h), sel ? th.bar : th.strip);
			cv.frame(int(b.x), int(b.y), int(b.w), int(b.h), sel ? th.tab_sel : th.panel_edge);
			const char *t = yes ? "Yes" : "No";
			put(int(b.x + b.w / 2) - wide(t) / 2, int(b.y) + 4, t, sel ? th.text_focus : th.text);
		}
	}
}

} // namespace ui
