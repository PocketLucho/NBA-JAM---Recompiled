// uitest.cpp - tests of the menu toolkit (src/app/ui). No ROM, no SDL, no display.
// usage: uitest [out_dir]     (exit code 0 = all passed; with out_dir, PNGs of the gallery are written)
//
// What is checked:
//   U1.1  every widget draws; nothing is drawn outside its panel
//   U1.2  the automated walk: from the start, every item that can take focus is reached with
//         directions and tab buttons alone, focus never rests on a header or a disabled item,
//         and Back is reported from every state
//   U1.3  keyboard, controller buttons, the stick, mouse and touch all drive the same screen;
//         hold-to-repeat speeds up
//   U1.4  the prompts follow the device used last
//   U1.5  text that does not fit is shortened or scrolled, never longer than its space
#include "ui/gallery.h"
#include "ui/themes.h"
#include "ui/screens.h"
#include <cmath>
#include "miniz.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <tuple>

using namespace ui;

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (line %d)\n", std::string(msg).c_str(), __LINE__); } else printf("ok    %s\n", std::string(msg).c_str()); } while (0)

static event ev(act a) { event e; e.a = a; return e; }
static event ptr(event::ptr_t p, float x, float y, float amount = 0) { event e; e.ptr = p; e.x = x; e.y = y; e.amount = amount; return e; }

static void write_png(const std::string &dir, const std::string &name, const canvas &cv)
{
	if (dir.empty()) return;
	std::vector<uint8_t> rgb(size_t(cv.w) * cv.h * 3);
	for (size_t i = 0; i < cv.px.size(); i++) { rgb[i * 3] = uint8_t(cv.px[i] >> 16); rgb[i * 3 + 1] = uint8_t(cv.px[i] >> 8); rgb[i * 3 + 2] = uint8_t(cv.px[i]); }
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), cv.w, cv.h, 3, &len);
	FILE *f = fopen((dir + "/" + name).c_str(), "wb");
	if (f && png) { fwrite(png, 1, len, f); }
	if (f) fclose(f);
	mz_free(png);
}

// every usable item of the screen, as (tab, panel, index)
static std::set<std::tuple<int, int, int>> all_usable(const screen &s)
{
	std::set<std::tuple<int, int, int>> out;
	for (int t = 0; t < int(s.tabs.size()); t++)
		for (int p = 0; p < int(s.tabs[size_t(t)].panels.size()); p++)
			for (int i = 0; i < int(s.tabs[size_t(t)].panels[size_t(p)].items.size()); i++)
				if (s.tabs[size_t(t)].panels[size_t(p)].items[size_t(i)].focusable()) out.insert({ t, p, i });
	return out;
}

int main(int argc, char **argv)
{
	std::string out = argc > 1 ? argv[1] : "";
	theme th;

	// ---------------- U1.2: the automated walk ----------------
	{
		screen start = make_gallery();
		static const act moves[] = { act::up, act::down, act::left, act::right, act::tab_prev, act::tab_next, act::page_up, act::page_down };
		std::set<std::tuple<int, int, int>> seen;
		std::vector<screen> todo = { start };
		bool focus_always_ok = true, back_always = true, never_on_bad = true;
		int states = 0;
		while (!todo.empty())
		{
			screen s = todo.back();
			todo.pop_back();
			const item *f = s.focused();
			int fi = s.tabs[size_t(s.cur_tab)].panels.empty() ? -1 : s.tabs[size_t(s.cur_tab)].panels[size_t(s.cur_panel)].focus;
			auto key = std::make_tuple(s.cur_tab, s.cur_panel, fi);
			if (seen.count(key)) continue;
			seen.insert(key);
			states++;
			focus_always_ok = focus_always_ok && s.focus_ok();
			if (f && s.cur_tab != 3) never_on_bad = never_on_bad && f->focusable();
			{ screen b = s; back_always = back_always && b.handle(ev(act::back)).what == result::back; }
			for (act m : moves) { screen n = s; n.handle(ev(m)); todo.push_back(n); }
		}
		auto want = all_usable(start);
		int missing = 0;
		for (auto &w : want) if (!seen.count(w)) { missing++; printf("      not reached: tab %d panel %d item %d\n", std::get<0>(w), std::get<1>(w), std::get<2>(w)); }
		CHECK(missing == 0, "walk: all " + std::to_string(want.size()) + " usable items of the gallery are reached with directions and tab buttons alone");
		CHECK(focus_always_ok && never_on_bad, "walk: focus is never lost and never on a header or a disabled item (" + std::to_string(states) + " states)");
		CHECK(back_always, "walk: Back is reported from every state");
		bool on_text_tab = false;
		for (auto &k : seen) on_text_tab = on_text_tab || std::get<0>(k) == 3;
		CHECK(on_text_tab, "walk: a tab with nothing to select can be entered and left");
	}

	// ---------------- the rule, case by case ----------------
	{
		screen s = make_gallery();
		CHECK(s.focused() && s.focused()->id == g_play, "focus starts on the first usable item, not on the header above it");
		s.handle(ev(act::down)); s.handle(ev(act::down));
		CHECK(s.focused()->id == g_toast, "Down moves to the next item");
		s.handle(ev(act::down));
		CHECK(s.focused()->id == 91, "Down skips a header and a disabled item");
		s.handle(ev(act::down));
		CHECK(s.focused()->id == 91, "Down at the bottom, with no panel below: focus stays");
		s.handle(ev(act::right));
		CHECK(s.cur_panel != 0 && s.focused()->focusable(), "Right from a button goes to the panel on that side");
		screen t = make_gallery();
		t.handle(ev(act::right));
		CHECK(t.focused()->id == g_music, "...and lands level with where it came from (second row to second row)");
		result r = t.handle(ev(act::right));
		CHECK(r.what == result::changed && r.id == g_music && t.find(g_music)->on && t.focused()->id == g_music, "Right on a toggle changes it and does not move");
		t.handle(ev(act::down));
		r = t.handle(ev(act::left));
		CHECK(r.what == result::changed && t.find(g_shader)->choice == 2, "Left on a picker steps back");
		for (int i = 0; i < 3; i++) t.handle(ev(act::right));
		CHECK(t.find(g_shader)->choice == 0, "a picker wraps round");
		t.handle(ev(act::down));
		for (int i = 0; i < 5; i++) t.handle(ev(act::right));
		CHECK(t.find(g_volume)->value == 1.0f, "a slider stops at its maximum");
		for (int i = 0; i < 3; i++) t.handle(ev(act::left));
		CHECK(std::abs(t.find(g_volume)->value - 0.7f) < 1e-5f, "a slider moves in whole steps (0.7, not 0.70000005)");
		t.handle(ev(act::down)); t.handle(ev(act::down));
		CHECK(t.focused()->id == 93, "Down past the last usable item goes to the panel below (the disabled toggle is skipped)");
		t.handle(ev(act::up));
		CHECK(t.focused()->id == g_delay, "Up from there comes back to the last usable item above");
		r = t.handle(ev(act::confirm));
		CHECK(r.what == result::none, "Confirm on a slider does nothing");
		t.handle(ev(act::tab_next));
		CHECK(t.cur_tab == 1 && t.focused()->id == g_grid0, "the shoulder button goes to the next tab, focus on its first item");
		t.handle(ev(act::right)); t.handle(ev(act::down));
		CHECK(t.focused()->id == g_grid0 + 9, "in a grid, Down from above a disabled cell jumps over it");
		t.handle(ev(act::up)); t.handle(ev(act::left)); t.handle(ev(act::down));
		CHECK(t.focused()->id == g_grid0 + 4, "grid: Left and Down move by cell");
		t.handle(ev(act::right));
		CHECK(t.focused()->id == g_grid0 + 6, "grid: Right jumps over a disabled cell in the row");
		t.handle(ev(act::down)); t.handle(ev(act::down));
		CHECK(t.focused()->id == 95, "grid: Down from the last row goes to the panel below");
		t.handle(ev(act::tab_prev)); t.handle(ev(act::tab_prev));
		CHECK(t.cur_tab == 3, "tabs wrap round");
		CHECK(t.handle(ev(act::confirm)).what == result::none && t.handle(ev(act::down)).what == result::none && t.focus_ok(), "a tab with nothing to select takes any input without harm");
		t.handle(ev(act::tab_prev));
		CHECK(t.cur_tab == 2 && t.focused()->id == g_list0, "long list: focus on the first item under the first header");
		t.handle(ev(act::page_down));
		int after_page = t.focused()->id;
		CHECK(after_page > g_list0 + 8, "Page down moves about a panel of items (now on item " + std::to_string(after_page - g_list0 + 1) + ")");
		for (int i = 0; i < 60; i++) t.handle(ev(act::down));
		CHECK(t.focused()->id == g_list0 + 39, "holding Down ends on the last item");
		const panel &lp = t.tabs[2].panels[0];
		CHECK(lp.focus / 1 >= lp.scroll && lp.focus < lp.scroll + t.rows_visible(0), "the list has scrolled so that the focused item is visible");
		for (int i = 0; i < 60; i++) t.handle(ev(act::up));
		CHECK(t.focused()->id == g_list0 && t.tabs[2].panels[0].scroll == 0, "back at the top the first header is in view again");
	}

	// ---------------- dialog and toast ----------------
	{
		screen s = make_gallery();
		s.handle(ev(act::down));
		gallery_react(s, s.handle(ev(act::confirm)), 0);
		CHECK(s.asking(), "a button opens the confirm dialog");
		CHECK(s.handle(ev(act::down)).what == result::none && s.focused()->id == g_ask, "while it is open the screen behind does not move");
		result r = s.handle(ev(act::confirm));
		CHECK(r.what == result::no && r.id == g_ask && !s.asking(), "the dialog starts on No: Confirm at once answers No");
		s.ask("Sure?", 7);
		s.handle(ev(act::left));
		r = s.handle(ev(act::confirm));
		CHECK(r.what == result::yes && r.id == 7, "Left, then Confirm answers Yes");
		s.ask("Sure?", 8);
		r = s.handle(ev(act::back));
		CHECK(r.what == result::no && !s.asking(), "Back in a dialog means No and closes it");
		CHECK(s.handle(ev(act::back)).what == result::back, "...and the next Back leaves the screen");
	}

	// ---------------- U1.3: every kind of input drives it ----------------
	{
		// keyboard
		screen s = make_gallery();
		input in;
		auto pump = [&](screen &sc) { event e; result last; while (in.next(e)) { result r = sc.handle(e); if (r.what != result::none) last = r; } return last; };
		raw_event k; k.kind = raw_event::key_down; k.k = key::down;
		in.feed(k, 0); k.kind = raw_event::key_up; in.feed(k, 10);
		pump(s);
		CHECK(s.focused()->id == g_ask && in.last_device() == device::keyboard, "keyboard: Down arrow moves");
		k.kind = raw_event::key_down; k.k = key::e; in.feed(k, 20); pump(s);
		CHECK(s.cur_tab == 1, "keyboard: E is next tab");
		k.k = key::tab; k.shift = true; in.feed(k, 30); pump(s);
		CHECK(s.cur_tab == 0, "keyboard: Shift+Tab is previous tab");
		k.shift = false; k.k = key::escape; in.feed(k, 40);
		CHECK(pump(s).what == result::back, "keyboard: Esc is Back");

		// controller buttons and stick
		raw_event p; p.kind = raw_event::pad_down; p.dev = device::playstation; p.p = pad::r1;
		in.feed(p, 100); pump(s);
		CHECK(s.cur_tab == 1 && in.last_device() == device::playstation, "controller: R1 is next tab, and the device is remembered");
		p.p = pad::south; in.feed(p, 110);
		CHECK(pump(s).what == result::activated, "controller: the bottom button confirms");
		p.p = pad::east; in.feed(p, 120);
		CHECK(pump(s).what == result::back, "controller: the right button is Back");
		raw_event st; st.kind = raw_event::pad_stick; st.dev = device::xbox; st.x = 0.9f; st.y = 0;
		in.feed(st, 200); st.x = 0.0f; in.feed(st, 210); pump(s);
		CHECK(s.focused()->id == g_grid0 + 1, "controller: the left stick moves once per push");
		st.x = 0.5f; in.feed(st, 220); st.x = 0.45f; in.feed(st, 230); st.x = 0; in.feed(st, 240); pump(s);
		CHECK(s.focused()->id == g_grid0 + 1, "controller: a stick inside the dead zone does nothing");

		// hold to repeat, speeding up
		screen l = make_gallery();
		l.handle(ev(act::tab_next)); l.handle(ev(act::tab_next));
		input rep;
		raw_event d; d.kind = raw_event::pad_down; d.p = pad::down;
		rep.feed(d, 1000);
		std::vector<unsigned> times;
		event e;
		for (unsigned t = 1000; t <= 3000; t++)
		{
			rep.tick(t);
			while (rep.next(e)) { times.push_back(t); l.handle(e); }
		}
		CHECK(times.size() > 20 && times[1] - times[0] >= input::kRepeatDelay, "holding a direction repeats after a pause (" + std::to_string(times.size()) + " moves in 2 s)");
		bool faster = true;
		for (size_t i = 3; i < times.size(); i++) faster = faster && (times[i] - times[i - 1]) <= (times[i - 1] - times[i - 2]) + 1;
		unsigned last_gap = times.back() - times[times.size() - 2];
		CHECK(faster && last_gap <= input::kRepeatMin + 1, "the repeat speeds up to one move every " + std::to_string(last_gap) + " ms");
		d.kind = raw_event::pad_up; rep.feed(d, 3001);
		for (unsigned t = 3001; t < 3600; t++) rep.tick(t);
		CHECK(!rep.next(e), "releasing stops it");

		// mouse
		screen m = make_gallery();
		input mi;
		auto slot_of = [&](screen &sc, int id) { for (auto &sl : sc.slots()) if (sc.tabs[size_t(sc.cur_tab)].panels[size_t(sl.panel)].items[size_t(sl.index)].id == id) return sl.r; return rectf{}; };
		rectf rt = slot_of(m, g_toast);
		raw_event mm; mm.kind = raw_event::mouse_move; mm.x = rt.x + 10; mm.y = rt.y + 5;
		mi.feed(mm, 0);
		while (mi.next(e)) m.handle(e);
		CHECK(m.focused()->id == g_toast && mi.last_device() == device::mouse, "mouse: hovering an item gives it the focus");
		mm.kind = raw_event::mouse_down; mi.feed(mm, 10); mm.kind = raw_event::mouse_up; mi.feed(mm, 20);
		result mr; while (mi.next(e)) { result r = m.handle(e); if (r.what != result::none) mr = r; }
		CHECK(mr.what == result::activated && mr.id == g_toast, "mouse: clicking presses it");
		rectf rd = slot_of(m, 90);
		mm.kind = raw_event::mouse_move; mm.x = rd.x + 10; mm.y = rd.y + 5; mi.feed(mm, 30);
		while (mi.next(e)) m.handle(e);
		CHECK(m.focused()->id == g_toast, "mouse: a disabled item does not take the focus");
		rectf tr = m.tab_rect(2);
		mm.kind = raw_event::mouse_down; mm.x = tr.x + 4; mm.y = tr.y + 4; mi.feed(mm, 40); mm.kind = raw_event::mouse_up; mi.feed(mm, 50);
		while (mi.next(e)) m.handle(e);
		CHECK(m.cur_tab == 2, "mouse: clicking a tab opens it");
		raw_event wh; wh.kind = raw_event::wheel; wh.y = -2;
		rectf pr = m.panel_rect(0);
		mm.kind = raw_event::mouse_move; mm.x = pr.x + 20; mm.y = pr.y + 60; mi.feed(mm, 55);
		mi.feed(wh, 60);
		while (mi.next(e)) m.handle(e);
		CHECK(m.tabs[2].panels[0].scroll == 6 && m.focus_ok(), "mouse: the wheel scrolls the list, and focus stays on something visible");
		// slider by click
		screen sl = make_gallery();
		rectf rv = slot_of(sl, g_volume);
		result sr = sl.handle(ptr(event::click, rv.x + rv.w * 0.55f + 1, rv.y + 5));
		CHECK(sr.what == result::changed && sl.find(g_volume)->value == 0.0f, "mouse: clicking the left end of a slider sets its minimum");

		// touch
		screen tt = make_gallery();
		input ti;
		rectf ra = slot_of(tt, g_ask);
		raw_event td; td.kind = raw_event::touch_down; td.x = ra.x + 10; td.y = ra.y + 5; ti.feed(td, 0);
		td.kind = raw_event::touch_up; ti.feed(td, 50);
		result trr; while (ti.next(e)) { result r = tt.handle(e); if (r.what != result::none) trr = r; }
		CHECK(trr.what == result::activated && trr.id == g_ask && ti.last_device() == device::touch, "touch: a tap presses the item under the finger");
		tt.handle(ev(act::tab_next)); tt.handle(ev(act::tab_next));
		rectf lpn = tt.panel_rect(0);
		td.kind = raw_event::touch_down; td.x = lpn.x + 30; td.y = lpn.y + 150; ti.feed(td, 100);
		for (int i = 1; i <= 10; i++) { td.kind = raw_event::touch_move; td.y = lpn.y + 150 - i * 6.0f; ti.feed(td, 100 + unsigned(i) * 10); }
		td.kind = raw_event::touch_up; ti.feed(td, 300);
		bool pressed = false;
		while (ti.next(e)) pressed = (tt.handle(e).what == result::activated) || pressed;
		CHECK(tt.tabs[2].panels[0].scroll == 5 && !pressed, "touch: dragging 60 pixels scrolls 5 rows and presses nothing");
	}

	// ---------------- U1.4: prompts ----------------
	CHECK(glyph(device::xbox, act::confirm) == "A" && glyph(device::playstation, act::confirm) == "Cross" && glyph(device::nintendo, act::confirm) == "B" && glyph(device::keyboard, act::confirm) == "Enter",
	      "prompts: Confirm is A, Cross, B or Enter by device");
	CHECK(glyph(device::xbox, act::tab_next) == "RB" && glyph(device::playstation, act::tab_next) == "R1" && glyph(device::nintendo, act::tab_next) == "R" && glyph(device::keyboard, act::tab_next) == "E",
	      "prompts: next tab is RB, R1, R or E");
	CHECK(glyph(device::touch, act::confirm) == "Tap" && glyph(device::mouse, act::confirm) == "Click", "prompts: touch and mouse have their own word for Confirm");

	// ---------------- U1.5: text ----------------
	{
		std::string longs = "A label that is far too long for its row";
		bool ok = true;
		for (int n = 0; n <= 45; n++)
		{
			ok = ok && int(fit(longs, n).size()) <= n;
			for (unsigned t = 0; t < 12000; t += 37) ok = ok && int(marquee(longs, n, t).size()) <= n;
		}
		CHECK(ok, "text: fit and marquee never return more characters than there is room for");
		CHECK(fit(longs, 12) == "A label th.." && fit("Short", 12) == "Short" && fit("Trailing space cut", 10) == "Trailing..", "text: a shortened text ends in two dots, a short one is untouched");
		bool start = false, end = false;
		for (unsigned t = 0; t < 12000; t += 10) { std::string m = marquee(longs, 20, t); start = start || m == longs.substr(0, 20); end = end || m == longs.substr(longs.size() - 20); }
		CHECK(start && end, "text: the marquee shows the beginning and reaches the end");
	}

	// ---------------- U1.1: drawing ----------------
	{
		screen s = make_gallery();
		for (int t = 0; t < int(s.tabs.size()); t++)
		{
			canvas cv(screen::kW, screen::kH);
			s.cur_tab = t; s.cur_panel = 0; s.fix_focus();
			s.draw(cv, th, t == 1 ? device::playstation : t == 2 ? device::nintendo : t == 3 ? device::keyboard : device::xbox, 0);
			static const char *names[] = { "gallery_1_widgets.png", "gallery_2_grid.png", "gallery_3_long_list.png", "gallery_4_text.png" };
			write_png(out, names[t], cv);
		}
		// nothing spills out of a panel: the gap between the two columns of tab 1 holds only background
		screen a = make_gallery(), b = make_gallery();
		for (auto &p : b.tabs[0].panels) p.items.clear();
		canvas ca(screen::kW, screen::kH), cb(screen::kW, screen::kH);
		a.tabs[0].panels[0].focus = 7;                      // the over-long label, focused, mid-scroll
		a.draw(ca, th, device::xbox, 2500); b.draw(cb, th, device::xbox, 2500);
		rectf l = a.panel_rect(0), r = a.panel_rect(1);
		bool clean = true;
		for (int x = int(l.x + l.w); x < int(r.x); x++) for (int y = screen::kTop; y < screen::kBottom; y++) clean = clean && ca.px[size_t(y) * ca.w + x] == cb.px[size_t(y) * cb.w + x];
		CHECK(int(r.x) - int(l.x + l.w) >= 4 && clean, "drawing: an over-long label stays inside its panel");
		bool differs = false;
		for (size_t i = 0; i < ca.px.size(); i++) differs = differs || ca.px[i] != cb.px[i];
		CHECK(differs, "drawing: items are drawn");
		// focus bar: the focused row is drawn differently from the same row unfocused
		screen f1 = make_gallery(), f2 = make_gallery();
		f2.handle(ev(act::down));
		canvas c1(screen::kW, screen::kH), c2(screen::kW, screen::kH);
		f1.draw(c1, th, device::xbox, 0); f2.draw(c2, th, device::xbox, 0);
		rectf row = f1.slots()[1].r;
		CHECK(c1.px[size_t(row.y + 6) * c1.w + size_t(row.x + row.w / 2)] != c2.px[size_t(row.y + 6) * c2.w + size_t(row.x + row.w / 2)], "drawing: the highlight bar follows the focus");
		// dialog and toast
		screen d = make_gallery();
		d.toast("Saved to nbajam_recomp.ini", 0);
		canvas ct(screen::kW, screen::kH);
		d.draw(ct, th, device::keyboard, 100);
		write_png(out, "gallery_5_toast.png", ct);
		d.ask("Reset the game? Anything not saved in the game's own memory is lost.", 1);
		canvas cd(screen::kW, screen::kH);
		d.draw(cd, th, device::xbox, 5000);
		write_png(out, "gallery_6_dialog.png", cd);
		bool dimmed = (cd.px[5 * cd.w + 5] & 0xff) < (ct.px[5 * ct.w + 5] & 0xff);
		CHECK(dimmed, "drawing: the dialog dims the screen behind it");
		canvas cx(screen::kW, screen::kH);
		d.draw(cx, th, device::xbox, 5000);
		bool same = cx.px == cd.px;
		CHECK(same, "drawing: the same state and time give the same picture");
	}

	// ---------------- U2.1: the project lettering ----------------
	{
		std::string errs;
		for (auto &e : lettering_errors()) errs += " " + e;
		CHECK(errs.empty(), "lettering: every drawing is well formed" + errs);
		std::string missing;
		for (int c = 32; c < 127; c++)
		{
			if (c >= 'a' && c <= 'z') continue;
			for (face f : { face::note, face::list, face::title })
				if (!has_glyph(char(c), f)) missing += char(c);
		}
		CHECK(missing.empty(), "lettering: capitals, digits and all ASCII punctuation are drawn in the three sizes" + (missing.empty() ? "" : ", missing: " + missing));
		CHECK(text_h(face::note) == 7 && text_h(face::list) == 8 && text_h(face::title) == 16, "lettering: three sizes, 7, 8 and 16 pixels high");
		CHECK(text_w("start a game", face::list) == text_w("START A GAME", face::list) && !has_glyph('a', face::list), "lettering: lower case is shown as capitals");
		CHECK(text_w("\xc3\xb1", face::list) == text_w("??", face::list) && !has_glyph('\xf1', face::list), "lettering: a character it does not have is shown as '?' and reported by has_glyph");
		CHECK(text_w("", face::list) == 0 && text_w("I", face::list) < text_w("M", face::list) && text_w("AB", face::title) == 2 * text_w("AB", face::list), "lettering: widths are per glyph, and the title is twice the list size");

		// drawn text: a gradient from top to bottom inside a dark outline, and nothing outside its box
		const ink k = { 0xffffec60, 0xffff5a10, 0xff000000 };
		bool boxed = true, graded = true, outlined = true;
		for (face f : { face::note, face::list, face::title })
		{
			canvas cv(200, 60);
			std::fill(cv.px.begin(), cv.px.end(), 0xff123456);
			const std::string s = "HE'S ON FIRE 42";
			int x0 = 20, y0 = 20, w = text_w(s, f), h = text_h(f), far = f == face::title ? 2 : 1;
			draw_text(cv, x0, y0, s, f, k);
			int out = 0;
			for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++)
			{
				uint32_t c = cv.px[size_t(y) * cv.w + x];
				if (c == 0xff123456) continue;
				if (x < x0 - 1 || y < y0 - 1 || x >= x0 + w + far || y >= y0 + h + far) boxed = false;
				if (c == k.outline) out++;
			}
			outlined = outlined && out > 0;
			// the stem of the H: red falls from the top row to the bottom row
			auto green = [&](int row) { return int((cv.px[size_t(y0 + row) * cv.w + size_t(x0 + (f == face::title ? 1 : 0))] >> 8) & 0xff); };
			graded = graded && green(1) > green(h / 2) && green(h / 2) > green(h - 2);
		}
		CHECK(boxed, "lettering: text stays inside its measured box plus the outline");
		CHECK(outlined && graded, "lettering: letters have an outline and a top-to-bottom gradient");

		std::string longs = "A label that is far too long for its row";
		bool ok = true, start = false, end = false;
		for (face f : { face::note, face::list, face::title })
			for (int px = 0; px <= 330; px += 7)
			{
				ok = ok && text_w(fit_px(longs, px, f), f) <= px;
				for (unsigned t = 0; t < 12000; t += 190) ok = ok && text_w(marquee_px(longs, px, f, t), f) <= px;
				if (px >= 24) for (auto &l : wrap_px(longs, px, f, 3)) ok = ok && text_w(l, f) <= px;
			}
		CHECK(ok, "lettering: fitted, scrolling and wrapped text is never wider than its space");
		for (unsigned t = 0; t < 12000; t += 10)
		{
			std::string m = marquee_px(longs, 120, face::list, t);
			start = start || longs.compare(0, m.size(), m) == 0;
			end = end || (m.size() <= longs.size() && longs.compare(longs.size() - m.size(), m.size(), m) == 0);
		}
		std::string f12 = fit_px(longs, 90, face::list);
		CHECK(start && end && f12.size() > 4 && f12.substr(f12.size() - 2) == ".." && fit_px("SHORT", 90, face::list) == "SHORT", "lettering: the marquee shows the beginning and reaches the end; a shortened text ends in two dots");
		auto lines = wrap_px("Reset the game? Anything not saved in the game's own memory is lost.", 240, face::list, 4);
		std::string joined;
		for (auto &l : lines) joined += (joined.empty() ? "" : " ") + l;
		CHECK(lines.size() > 1 && joined == "Reset the game? Anything not saved in the game's own memory is lost.", "lettering: wrapping breaks at spaces and loses no word");

		canvas sheet(screen::kW, screen::kH);
		for (int y = 0; y < sheet.h; y++) sheet.fill(0, y, sheet.w, 1, 0xff14102c);
		draw_sheet(sheet, { th.title_top, th.title_bottom, th.outline }, th.ink_for(th.text));
		write_png(out, "font_sheet.png", sheet);
		// the gallery is drawn with it: the title area holds the title's two gradient ends
		screen g = make_gallery();
		canvas cg(screen::kW, screen::kH);
		g.draw(cg, th, device::xbox, 0);
		bool warm = false, hot = false;
		for (int y = 2; y < 21; y++) for (int x = 0; x < cg.w; x++)
		{
			uint32_t c = cg.px[size_t(y) * cg.w + x];
			int r = (c >> 16) & 0xff, gr = (c >> 8) & 0xff, b = c & 0xff;
			warm = warm || (y < 8 && r > 240 && gr > 200 && b < 140);
			hot = hot || (y > 14 && r > 230 && gr < 140 && b < 60);
		}
		CHECK(warm && hot, "lettering: the gallery's title is drawn in the project lettering with the theme's gradient");
	}

	// ---------------- U2.3: themes ----------------
	{
		const std::vector<std::string> &ids = theme_ids();
		CHECK(ids.size() == 5 && ids[0] == "neutral", "themes: neutral and one per game family are built in");
		std::string bad;
		for (auto &id : ids)
		{
			std::vector<std::string> problems;
			theme_spec sp = parse_theme(id, builtin_theme(id), &problems);
			for (auto &p : problems) bad += " " + p;
			if (sp.name.empty() || sp.name == id) bad += " " + id + ": no name";
		}
		CHECK(bad.empty(), "themes: every built-in theme file reads without a problem" + bad);
		CHECK(theme_id_for_game("nbajam") == "nbajam" && theme_id_for_game("nbajamte") == "te" && theme_id_for_game("nbajamse") == "te" &&
		      theme_id_for_game("nbahangt") == "hangtime" && theme_id_for_game("nbamht") == "mht" && theme_id_for_game("pong") == "neutral",
		      "themes: each game has its family's theme; T.E. and SE share one; an unknown game is neutral");
		theme neutral;
		theme_spec n = parse_theme("neutral", builtin_theme("neutral"));
		CHECK(n.colours.bar == neutral.bar && n.colours.bg_top == neutral.bg_top && n.colours.title_bottom == neutral.title_bottom && n.colours.text == neutral.text,
		      "themes: the neutral file holds the toolkit's default colours");

		std::vector<std::string> problems;
		theme_spec sp = parse_theme("x", "name = Test ; a comment\nbar = #102030\n\ntext = red\nnonsense\nwhat = 3\npanel_alpha = 300\nbackground = 400x254\nbackground_crop = 1 2 3\nlist_font = 9\n", &problems);
		CHECK(sp.name == "Test" && sp.colours.bar == 0xff102030 && sp.bg_w == 400 && sp.bg_h == 254 && sp.list_font == 9, "themes: a file's lines are read, comments left out");
		CHECK(problems.size() == 5 && sp.colours.text == neutral.text && sp.colours.panel_alpha == 255 && sp.crop[2] == 0,
		      "themes: lines that make no sense are listed and skipped; those keys keep the neutral values (" + std::to_string(problems.size()) + " listed)");

		// without ROM art a game family's theme is the neutral one, whole
		theme_spec ht = parse_theme("hangtime", builtin_theme("hangtime"));
		theme plain = make_theme(ht, nullptr);
		rom_art::art_set none;
		theme plain2 = make_theme(ht, &none);
		CHECK(ht.colours.bar != neutral.bar && plain.bar == neutral.bar && plain.backdrop.empty() && !plain.title_font && !plain.list_font && plain2.bar == neutral.bar,
		      "themes: without ROM art the neutral theme is used whole");

		// cross-fade
		theme a = neutral, b = ht.colours;
		b.backdrop.assign(size_t(screen::kW) * screen::kH, 0xff204060);
		theme m0 = mix_themes(a, b, 0.0f), m1 = mix_themes(a, b, 1.0f), mh = mix_themes(a, b, 0.5f);
		auto red = [](uint32_t c) { return int((c >> 16) & 0xff); };
		bool halfway = std::abs(red(mh.bar) - (red(a.bar) + red(b.bar)) / 2) <= 1 && mh.backdrop.size() == b.backdrop.size();
		uint32_t top = mh.backdrop.empty() ? 0 : mh.backdrop[0];
		halfway = halfway && std::abs(red(top) - (red(a.bg_top) + 0x20) / 2) <= 1;
		CHECK(m0.bar == a.bar && m0.backdrop.empty() && m1.bar == b.bar && m1.backdrop == b.backdrop && halfway,
		      "themes: the cross-fade starts at one theme, ends at the other and mixes colours and backgrounds between");

		// a theme file in the data folder is read instead of the built-in one
		if (!out.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(out + "/themes", ec);
			std::string path = out + "/themes/te.theme";
			if (FILE *f = fopen(path.c_str(), "wb")) { fputs("name = Mine\nbar = #010203\n", f); fclose(f); }
			theme_spec mine = load_theme_spec("te", out), stock = load_theme_spec("hangtime", out);
			std::filesystem::remove(path, ec);
			std::filesystem::remove(out + "/themes", ec);
			CHECK(mine.name == "Mine" && mine.colours.bar == 0xff010203 && stock.name == "NBA Hangtime" && load_theme_spec("te", "").name == "NBA JAM Tournament Edition",
			      "themes: themes/<id>.theme in the data folder replaces the built-in file; the others stay built in");
		}

		// the gallery's theme tab
		std::vector<std::string> names;
		for (auto &id : ids) names.push_back(parse_theme(id, builtin_theme(id)).name);
		screen g = make_gallery(names);
		CHECK(make_gallery().tabs.size() == 4 && g.tabs.size() == 5 && g.tabs[4].panels[0].items.size() == 6, "themes: the gallery has a tab with a button per theme");
		g.cur_tab = 4; g.cur_panel = 0; g.fix_focus();
		g.handle(ev(act::down));
		result r = g.handle(ev(act::confirm));
		CHECK(r.what == result::activated && r.id == g_theme0 + 1, "themes: pressing a theme's button reports which one");
	}

	// ---------------- U2.3 / U2.4: a theme with ROM art, and glyphs the ROM font lacks ----------------
	{
		// a hand-made stand-in for a font read from a ROM: A, B and 1, 9 rows high, white
		// with a dark edge; it has no '#' and no '?'
		auto f = std::make_shared<rom_art::font>();
		f->cap_h = 9;
		for (char c : { 'A', 'B', '1', 'a', 'b' })
		{
			rom_art::picture &p = f->glyph[static_cast<unsigned char>(c)];
			p.w = 6; p.h = 9;
			p.px.assign(54, 0);
			for (int y = 0; y < 9; y++) for (int x = 0; x < 6; x++) p.px[size_t(y) * 6 + x] = (x == 0 || y == 0 || x == 5 || y == 8) ? 0xff202020 : (y < 5 ? 0xffffffff : 0xffc0c0ff);
		}
		ink k = ink_from(*f);
		CHECK(k.outline == 0xff202020 && k.top == 0xffffffff && (k.bottom & 0xff) == 0xff && ((k.bottom >> 16) & 0xff) < 0xff,
		      "missing glyphs: the colours of the fallback are taken from the game's font (outline, top, bottom)");
		CHECK(text_w("AB", *f) == 13 && text_w("A B", *f) > text_w("AB", *f) && text_w("A#B", *f) > text_w("AB", *f) + 2 && text_w("", *f) == 0,
		      "missing glyphs: a character the game's font lacks still takes its room");
		canvas cv(120, 30);
		std::fill(cv.px.begin(), cv.px.end(), 0xff123456);
		draw_text(cv, 10, 10, "A#B", *f);
		int w_a = 6, w_all = text_w("A#B", *f);
		int own = 0, borrowed = 0, lit = 0, rows_top = 99, rows_bot = -1;
		for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++)
		{
			uint32_t c = cv.px[size_t(y) * cv.w + x];
			if (c == 0xff123456) continue;
			if (x < 10 + w_a) own++;
			else if (x < 10 + w_all - 6 - 1) { borrowed++; if (c != k.outline) { lit++; rows_top = std::min(rows_top, y); rows_bot = std::max(rows_bot, y); } }
		}
		CHECK(own == 54 && borrowed > 20 && lit > 10, "missing glyphs: the missing character is drawn from the project lettering between the game's letters");
		CHECK(rows_top == 10 && rows_bot == 18, "missing glyphs: the borrowed character has the height of the game's capitals (rows " + std::to_string(rows_top) + " to " + std::to_string(rows_bot) + ")");
		bool whole = true;
		for (int c = 33; c < 127; c++)
		{
			canvas one(40, 30);
			std::fill(one.px.begin(), one.px.end(), 0xff123456);
			draw_text(one, 5, 5, std::string(1, char(c)), *f);
			bool any = false;
			for (uint32_t p : one.px) any = any || p != 0xff123456;
			whole = whole && any && text_w(std::string(1, char(c)), *f) > 0;
		}
		CHECK(whole, "missing glyphs: every printable character shows something in a game's font");
		canvas dimmed(120, 30);
		std::fill(dimmed.px.begin(), dimmed.px.end(), 0xff123456);
		draw_text(dimmed, 10, 10, "A", *f, 0xff808080);
		CHECK(dimmed.px[size_t(12) * dimmed.w + 12] == 0xff808080, "missing glyphs: a tint dims the game's letters (a disabled row)");

		// the gallery in a theme that has a background and the game's lettering
		theme art = th;
		art.backdrop.assign(size_t(screen::kW) * screen::kH, 0xff305070);
		art.title_font = f; art.list_font = f; art.panel_alpha = 128;
		screen s = make_gallery();
		canvas plain(screen::kW, screen::kH), themed(screen::kW, screen::kH);
		s.draw(plain, th, device::xbox, 0);
		s.draw(themed, art, device::xbox, 0);
		write_png(out, "gallery_8_rom_font_stand_in.png", themed);
		rectf p0 = s.panel_rect(0);
		uint32_t in_panel = themed.px[size_t(p0.y + p0.h - 4) * themed.w + size_t(p0.x + p0.w - 4)];
		CHECK(themed.px[2 * themed.w + 2] == 0xff305070 && in_panel != th.panel && in_panel != 0xff305070, "themes: the background is painted behind the screen and shows through the panels");
		// nothing spills: the gap between the two columns holds only background
		rectf l = s.panel_rect(0), r2 = s.panel_rect(1);
		bool clean = true;
		for (int x = int(l.x + l.w); x < int(r2.x); x++) for (int y = screen::kTop; y < screen::kBottom; y++) clean = clean && themed.px[size_t(y) * themed.w + x] == 0xff305070;
		CHECK(clean && themed.px != plain.px, "themes: in a game's lettering the rows still stay inside their panels");
	}

	// ---------------- the screens not rebuilt yet: old layout, new lettering ----------------
	{
		canvas cv(200, 40);
		std::fill(cv.px.begin(), cv.px.end(), 0xff123456);
		cv.text(8, 8, "AB I", 0xffffffff);
		// each character keeps its 8-pixel cell: nothing left of the first cell's outline,
		// the third cell (a space) empty, the I inside the fourth
		bool cells = true, third_empty = true, fourth = false;
		for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++)
		{
			bool ink_here = cv.px[size_t(y) * cv.w + x] != 0xff123456;
			if (ink_here && (x < 7 || x > 8 + 32 || y < 7 || y > 16)) cells = false;
			if (ink_here && x >= 8 + 17 && x < 8 + 23) third_empty = false;
			if (ink_here && x >= 8 + 24 && x < 8 + 32) fourth = true;
		}
		CHECK(cells && third_empty && fourth && cv.text_w("AB I") == 32, "old screens: text keeps its 8-pixel cells, so every old layout holds");
		bool outlined = false, graded = false;
		for (uint32_t p : cv.px) { outlined = outlined || p == 0xff000000; graded = graded || (p != 0xff000000 && p != 0xff123456 && p != 0xffffffff); }
		CHECK(outlined && graded, "old screens: their text is drawn in the project lettering (outline and gradient)");
		canvas big(200, 60);
		std::fill(big.px.begin(), big.px.end(), 0xff123456);
		big.text(8, 8, "A", 0xffff8020, 4);
		int top = 99, bottom = -1;
		for (int y = 0; y < big.h; y++) for (int x = 0; x < big.w; x++) if (big.px[size_t(y) * big.w + x] != 0xff123456 && big.px[size_t(y) * big.w + x] != 0xff000000) { top = std::min(top, y); bottom = std::max(bottom, y); }
		CHECK(top == 8 && bottom == 8 + 31, "old screens: text four cells high is the title lettering doubled");
		// a file name keeps its lower case: drawn in the plain font, not as capitals
		canvas a(200, 40), b(200, 40), c2(200, 40);
		a.text(8, 8, "nbajam_recomp.ini", 0xffffffff);
		b.text_plain(8, 8, "nbajam_recomp.ini", 0xffffffff);
		c2.text(8, 8, "Esc: back", 0xffffffff);
		canvas d(200, 40);
		d.text_plain(8, 8, "Esc: back", 0xffffffff);
		canvas sample(400, 300);
		sample.text(sample.w / 2 - sample.text_w("NBA JAM", 4) / 2, 8, "NBA JAM", 0xffff8020, 4);
		sample.text(sample.w / 2 - sample.text_w("RECOMP", 2) / 2, 42, "RECOMP", 0xffffffff, 2);
		int sy = sample.text_wrap(12, 70, "Data (portable): C:/Users/someone/Games/NBAJamRecomp", 0xffa0a0c0);
		sy = sample.text_wrap(12, sy + 3, "A recompilation of Midway's NBA JAM saga from the original source code that was made public.", 0xffd8e0ff);
		sy = sample.text_wrap(12, sy + 3, "Built from source:", 0xff80ff80);
		sy = sample.text_wrap(12, sy, " T.E. rev 4.0 (identical to the final ROM)", 0xffd8e0ff);
		sy = sample.text_wrap(12, sy, "Sources: github.com/historicalsource and Asure", 0xffb0b0b0);
		sample.text(8, sample.h - 11, "Enter / Esc: back (saved to mods.ini)", 0xffb0b0b0);
		write_png(out, "old_screens_sample.png", sample);
		CHECK(a.px == b.px && c2.px != d.px, "old screens: file names and web addresses keep their lower case; other words use the lettering");
	}

	// ---------------- U3.1 / U3.2: the game picker and Settings ----------------
	{
		picker_state st;
		const char *titles[5] = { "NBA JAM", "NBA JAM Tournament Edition", "NBA JAM T.E. Special Edition", "NBA Hangtime", "NBA Maximum Hangtime" };
		for (int i = 0; i < 5; i++)
		{
			picker_game g;
			g.title = titles[i]; g.revision = "1994 - rev 4.0 (source build = ROM)"; g.board = i < 3 ? "T-Unit" : "Wolf Unit";
			g.big = { "NBA JAM", "T.E." };
			g.available = i != 3; g.from_source = i == 1;
			g.why = "Copy the MAME romset nbahangt.zip to roms/";
			st.games.push_back(g);
		}
		st.mods_on = 1; st.mods_total = 2; st.version = "1.2.3"; st.roms_dir = "C:/Users/someone/AppData/Roaming/NBAJamRecomp/roms";
		settings_state ss;
		ss.shaders = { "Sharp (pixel perfect)", "Smooth", "Scanlines", "CRT", "Curved CRT" };
		ss.side_names = { "Black", "Blurred picture", "arcade-border" };

		// the U1.2 walk on both screens: every usable item reached by directions and tab
		// buttons alone, focus never lost, Back reported everywhere
		auto walk = [&](const screen &start, const std::string &name)
		{
			static const act moves[] = { act::up, act::down, act::left, act::right, act::tab_prev, act::tab_next, act::page_up, act::page_down };
			std::set<std::tuple<int, int, int>> seen;
			std::vector<screen> todo = { start };
			bool ok = true, back = true;
			while (!todo.empty())
			{
				screen s = todo.back();
				todo.pop_back();
				auto key = std::make_tuple(s.cur_tab, s.cur_panel, s.tabs[size_t(s.cur_tab)].panels[size_t(s.cur_panel)].focus);
				if (seen.count(key)) continue;
				seen.insert(key);
				ok = ok && s.focus_ok() && (!s.focused() || s.focused()->focusable());
				{ screen b = s; back = back && b.handle(ev(act::back)).what == result::back; }
				for (act m : moves) { screen n = s; n.handle(ev(m)); todo.push_back(n); }
			}
			int missing = 0;
			auto want = all_usable(start);
			for (auto &w : want) missing += !seen.count(w);
			CHECK(missing == 0 && ok && back, name + ": all " + std::to_string(want.size()) + " usable items are reached with directions and tab buttons alone; focus is never lost; Back is always reported");
		};
		screen pk = make_picker(st);
		walk(pk, "picker");
		screen se = make_settings(ss);
		walk(se, "settings");

		// picker: what it shows
		CHECK(pk.focused() && pk.focused()->id == pk_game0 && pk.find(pk_game0 + 3)->dim && pk.find(pk_game0 + 3)->focusable() && pk.find(pk_game0 + 3)->value_text == "No ROMs" && pk.find(pk_game0)->value_text.empty() && pk.rows_visible(1) >= 8 && pk.rows_visible(0) >= 5,
		      "picker: the games are listed; one without ROMs is greyed but can still be highlighted; list and menu show every row");
		auto side = [&](const screen &s) { std::string all; for (auto &i : s.tabs[0].panels[2].items) all += i.label + " "; return all; };
		update_picker(pk, st, 0);
		std::string ready = side(pk);
		for (int i = 0; i < 3; i++) pk.handle(ev(act::down));
		int at = picker_game_index(pk, 0);
		update_picker(pk, st, at);
		std::string missing_rom = side(pk);
		CHECK(at == 3 && ready.find("READY TO PLAY") != std::string::npos && missing_rom.find("CANNOT BE PLAYED YET") != std::string::npos && missing_rom.find("nbahangt.zip") != std::string::npos &&
		      missing_rom.find("Version 1.2.3") != std::string::npos, "picker: the side panel follows the highlighted game and gives the reason when it cannot be played");
		result r = pk.handle(ev(act::confirm));
		CHECK(r.what == result::activated && r.id == pk_game0 + 3, "picker: pressing a game reports it, playable or not");
		for (int i = 0; i < 2; i++) pk.handle(ev(act::down));
		CHECK(pk.cur_panel == 1 && pk.focused()->id == pk_settings && picker_game_index(pk, at) == 3 && pk.find(pk_online)->disabled,
		      "picker: below the list is the menu; it remembers the game, and Online is off for a game without ROMs");
		CHECK(pk.find(pk_mods)->value_text == "1 of 2 on", "picker: the menu shows how many Lua mods are on");
		picker_state none = st;
		for (auto &g : none.games) g.available = false;
		screen first = make_picker(none);
		std::string fr = side(first);
		CHECK(fr.find("NO GAMES FOUND") != std::string::npos && fr.find("Look for ROMs again") != std::string::npos && fr.find("C:/Us") != std::string::npos,
		      "picker: with no game playable it says where the ROM sets go");
		picker_state news = st;
		news.news_new = true; news.news_tag = "v1.3.0";
		screen pn = make_picker(news);
		CHECK(side(pn).find("NEW VERSION AVAILABLE") != std::string::npos && pn.find(pk_news)->value_text == "New version", "picker: a new version is announced on the side panel and on the News entry");
		pk.handle(ev(act::back));
		CHECK(!pk.asking(), "picker: Back is left to the program (which asks before quitting)");

		// settings: the widgets and the state agree both ways
		ss.shape = 1; ss.scaling = 2; ss.shader = 4; ss.scanlines = 0.4f; ss.sides = 2; ss.fullscreen = true; ss.volume = 0.3f;
		ss.free_play = false; ss.four_players = false; ss.country = 2; ss.video_clips = false; ss.news = false;
		show_settings(se, ss);
		settings_state back2;
		back2.shaders = ss.shaders; back2.side_names = ss.side_names;
		read_settings(se, back2);
		CHECK(back2.shape == 1 && back2.scaling == 2 && back2.shader == 4 && std::fabs(back2.scanlines - 0.4f) < 0.01f && back2.sides == 2 && back2.fullscreen && std::fabs(back2.volume - 0.3f) < 0.01f &&
		      !back2.free_play && !back2.four_players && back2.country == 2 && !back2.video_clips && !back2.news, "settings: every value goes to its widget and back unchanged");
		se.cur_tab = 0; se.cur_panel = 0; se.fix_focus();
		result ch = se.handle(ev(act::right));
		read_settings(se, back2);
		CHECK(ch.what == result::changed && ch.id == st_shape && back2.shape == 0, "settings: changing a widget reports it and the new value is read back");
		CHECK(se.tabs.size() == 4 && se.tabs[0].name == "Video" && se.tabs[1].name == "Sound" && se.tabs[2].name == "Game", "settings: Video, Sound, Game and Program tabs");

		// pictures
		theme th2;
		canvas c1(screen::kW, screen::kH);
		screen shot = make_picker(st);
		update_picker(shot, st, 0);
		shot.draw(c1, th2, device::xbox, 0);
		draw_picker_name(c1, shot, th2, st.games[0], nullptr);
		write_png(out, "picker_1_ready.png", c1);
		for (int i = 0; i < 3; i++) shot.handle(ev(act::down));
		update_picker(shot, st, 3);
		canvas c2(screen::kW, screen::kH);
		shot.draw(c2, th2, device::keyboard, 0);
		draw_picker_name(c2, shot, th2, st.games[3], nullptr);
		write_png(out, "picker_2_no_roms_for_game.png", c2);
		canvas c3(screen::kW, screen::kH);
		first.draw(c3, th2, device::playstation, 0);
		draw_picker_name(c3, first, th2, none.games[0], nullptr);
		write_png(out, "picker_3_first_run.png", c3);
		for (int t = 0; t < 4; t++)
		{
			canvas c(screen::kW, screen::kH);
			se.cur_tab = t; se.cur_panel = 0; se.fix_focus();
			se.draw(c, th2, device::xbox, 0);
			write_png(out, "settings_" + std::to_string(t + 1) + ".png", c);
		}
		// the name is drawn inside the side panel
		rectf ir = shot.panel_rect(2);
		bool inside = true, drawn = false;
		canvas a(screen::kW, screen::kH), b(screen::kW, screen::kH);
		shot.draw(a, th2, device::xbox, 0); shot.draw(b, th2, device::xbox, 0);
		draw_picker_name(b, shot, th2, st.games[3], nullptr);
		for (int y = 0; y < a.h; y++) for (int x = 0; x < a.w; x++)
			if (a.px[size_t(y) * a.w + x] != b.px[size_t(y) * b.w + x]) { drawn = true; inside = inside && x > ir.x && x < ir.x + ir.w - 1 && y > ir.y && y < ir.y + 5 * screen::kRow + 2; }
		CHECK(drawn && inside, "picker: the game's name is drawn large at the top of the side panel, inside it");
	}

	// ---------------- U3.3 / U3.4: Controls and Lua mods ----------------
	{
		auto walk = [&](const screen &start, const std::string &name)
		{
			static const act moves[] = { act::up, act::down, act::left, act::right, act::tab_prev, act::tab_next, act::page_up, act::page_down };
			std::set<std::tuple<int, int, int>> seen;
			std::vector<screen> todo = { start };
			bool ok = true, back = true;
			while (!todo.empty())
			{
				screen s = todo.back();
				todo.pop_back();
				auto key = std::make_tuple(s.cur_tab, s.cur_panel, s.tabs[size_t(s.cur_tab)].panels[size_t(s.cur_panel)].focus);
				if (seen.count(key)) continue;
				seen.insert(key);
				ok = ok && s.focus_ok() && (!s.focused() || s.focused()->focusable());
				{ screen b = s; back = back && b.handle(ev(act::back)).what == result::back; }
				for (act m : moves) { screen n = s; n.handle(ev(m)); todo.push_back(n); }
			}
			int missing = 0;
			auto want = all_usable(start);
			for (auto &w : want) missing += !seen.count(w);
			CHECK(missing == 0 && ok && back, name + ": all " + std::to_string(want.size()) + " usable items are reached with directions alone; focus is never lost; Back is always reported");
		};

		// Lua mods
		mods_state ms;
		ms.folder = "C:/Users/someone/Games/NBAJamRecomp/mods";
		mod_view a;
		a.name = "Quarter length"; a.description = "Sets how long a quarter lasts, from 30 seconds to 12 minutes, on every game."; a.enabled = true;
		a.options.push_back({ "Minutes", { "0:30", "1:30", "3:00", "12:00" }, 1 });
		a.games = { { "NBA JAM", true, true }, { "NBA JAM Tournament Edition", true, true }, { "NBA Hangtime", false, false } };
		mod_view b;
		b.name = "No select timer"; b.description = "No countdown on team select."; b.enabled = false; b.all_games = false;
		b.games = a.games;
		mod_view c;
		c.name = "Broken script"; c.error = "broken.lua:3: unexpected symbol near 'then'";
		ms.mods = { a, b, c };
		screen md = make_mods(ms);
		walk(md, "Lua mods");
		auto page = [&](const screen &s) { std::string all; for (auto &i : s.tabs[0].panels[1].items) all += i.label + " "; return all; };
		CHECK(md.find(md_mod0)->value_text == "On" && md.find(md_mod0 + 1)->value_text == "Off" && md.find(md_mod0 + 1)->dim && md.find(md_mod0 + 2)->value_text == "Error",
		      "Lua mods: the list says which scripts are on, off or broken");
		std::string p0 = page(md);
		CHECK(p0.find("Sets how long a quarter lasts") != std::string::npos && p0.find("12 minutes") != std::string::npos && md.find(md_enabled) && md.find(md_enabled)->on && md.find(md_apply)->choice == 0 &&
		      md.find(md_opt0)->choices.size() == 4 && md.find(md_opt0)->choice == 1 && md.find(md_game0)->on && md.find(md_game0 + 2)->disabled,
		      "Lua mods: the page shows the whole description, the switch, the games it applies to and its options; an unsupported game cannot be switched");
		CHECK(mods_open_page(md) && md.cur_panel == 1 && md.focused()->id == md_enabled, "Lua mods: confirming a script goes to its page");
		result ch = md.handle(ev(act::confirm));
		CHECK(ch.what == result::changed && ch.id == md_enabled && !md.find(md_enabled)->on, "Lua mods: a change on the page is reported");
		md.cur_panel = 0;
		md.handle(ev(act::down));
		int at = mods_index(md, 0);
		update_mods(md, ms, at);
		CHECK(at == 1 && md.find(md_apply)->choice == 1 && md.tabs[0].panels[1].title == "No select timer", "Lua mods: the page follows the highlighted script");
		md.handle(ev(act::down));
		update_mods(md, ms, mods_index(md, at));
		CHECK(page(md).find("unexpected symbol") != std::string::npos && !md.find(md_enabled) && !mods_open_page(md), "Lua mods: a broken script shows its error and has nothing to change");
		mods_state none;
		none.folder = ms.folder;
		screen empty = make_mods(none);
		CHECK(page(empty).find("Put .lua scripts in this folder") != std::string::npos && empty.focus_ok(), "Lua mods: with no scripts it says where they go");

		// Controls
		controls_state cs;
		cs.games = { "All games", "NBA JAM", "NBA Hangtime" };
		cs.profiles = { "Default", "Fight stick" };
		cs.pad = "Xbox Wireless Controller";
		static const char *names[13] = { "Up", "Down", "Left", "Right", "Shoot", "Pass", "Turbo", "Start", "Coin", "Shove", "Turbo shot", "Shortcut 1", "Shortcut 2" };
		for (int i = 0; i < 13; i++) cs.actions.push_back({ names[i], i == 4 ? "a, y" : "dpup", i == 4 });
		cs.shortcut_sets = { "-", "Shoot", "Pass", "Turbo + Pass" };
		cs.shortcut[1] = 3;
		screen ct = make_controls(cs);
		walk(ct, "Controls");
		CHECK(ct.find(ct_act0 + 4)->value_text == "* a, y" && ct.find(ct_act0)->value_text == "dpup" && ct.find(ct_short0 + 1)->choice == 3 && ct.find(ct_profile)->choices.size() == 2 &&
		      ct.tabs[0].panels[1].title == "Buttons: Xbox Wireless Controller" && ct.rows_visible(1) >= 13,
		      "Controls: every action shows its binding, a held one is marked, and all thirteen fit without scrolling");
		cs.capturing = 5; cs.seconds = 4; cs.pad.clear(); cs.game = 2; cs.profiles = { "As all games: Default", "Default", "Fight stick" };
		update_controls(ct, cs);
		CHECK(ct.find(ct_act0 + 5)->value_text == "Press it... 4" && ct.tabs[0].panels[1].title == "Buttons (no controller)" && ct.find(ct_game)->choice == 2 &&
		      ct.find(ct_profile)->choices.size() == 3 && ct.find(ct_export_setup)->label == "Export this game's setup",
		      "Controls: an action being bound counts down; one game can follow the all-games profile");
		cs.capturing = -1; cs.naming = true; cs.name = "Stick copy";
		update_controls(ct, cs);
		CHECK(ct.find(ct_new)->label == "Name: Stick copy_", "Controls: the name of a new profile is shown while it is typed");
		ct.cur_panel = 1; ct.fix_focus();
		result pr = ct.handle(ev(act::confirm)), cl = ct.handle(ev(act::context));
		CHECK(pr.what == result::activated && pr.id == ct_act0 && cl.what == result::context && cl.id == ct_act0, "Controls: Confirm on an action asks for a press, the context button clears it");

		theme th2;
		screen shots[3] = { make_mods(ms), make_controls(cs), make_mods(none) };
		static const char *files[3] = { "mods_1_page.png", "controls_1.png", "mods_2_none.png" };
		for (int i = 0; i < 3; i++)
		{
			canvas cv(screen::kW, screen::kH);
			shots[i].draw(cv, th2, i == 1 ? device::xbox : device::keyboard, 0);
			write_png(out, files[i], cv);
		}
	}

	// ---------------- U3.5 / U3.6: Online, the lobby, About and help, News ----------------
	{
		auto walk = [&](const screen &start, const std::string &name)
		{
			static const act moves[] = { act::up, act::down, act::left, act::right, act::tab_prev, act::tab_next, act::page_up, act::page_down };
			std::set<std::tuple<int, int, int>> seen;
			std::vector<screen> todo = { start };
			bool ok = true, back = true;
			while (!todo.empty())
			{
				screen s = todo.back();
				todo.pop_back();
				auto key = std::make_tuple(s.cur_tab, s.cur_panel, s.tabs[size_t(s.cur_tab)].panels[size_t(s.cur_panel)].focus);
				if (seen.count(key)) continue;
				seen.insert(key);
				ok = ok && s.focus_ok();
				{ screen b = s; back = back && b.handle(ev(act::back)).what == result::back; }
				for (act m : moves) { screen n = s; n.handle(ev(m)); todo.push_back(n); }
			}
			int missing = 0;
			auto want = all_usable(start);
			for (auto &w : want) missing += !seen.count(w);
			CHECK(missing == 0 && ok && back, name + ": all " + std::to_string(want.size()) + " usable items are reached with directions alone; focus is never lost; Back is always reported");
		};
		auto text_of = [](const screen &s, int t, int p) { std::string all; for (auto &i : s.tabs[size_t(t)].panels[size_t(p)].items) all += i.label + " " + (i.value_text.empty() ? "" : i.value_text + " "); return all; };

		// a panel of text only is read by scrolling it
		{
			screen s;
			tab t;
			panel p;
			p.r = { 0, 0, 1, 1 };
			for (int i = 0; i < 60; i++) p.items.push_back(label("line " + std::to_string(i)));
			t.panels = { p };
			s.tabs = { t };
			s.fix_focus();
			const int rows = s.rows_visible(0);
			s.handle(ev(act::up));
			bool top = s.tabs[0].panels[0].scroll == 0;
			s.handle(ev(act::down)); s.handle(ev(act::down));
			bool two = s.tabs[0].panels[0].scroll == 2;
			s.handle(ev(act::page_down));
			bool page = s.tabs[0].panels[0].scroll == 2 + rows - 1;
			for (int i = 0; i < 100; i++) s.handle(ev(act::down));
			bool end = s.tabs[0].panels[0].scroll == 60 - rows;
			event wheel; wheel.ptr = event::scroll; wheel.x = 200; wheel.y = 150; wheel.amount = -3;
			s.handle(wheel);
			bool wh = s.tabs[0].panels[0].scroll == 60 - rows - 3;
			s.handle(ev(act::page_up));
			CHECK(top && two && page && end && wh && s.tabs[0].panels[0].scroll == 60 - rows - 3 - (rows - 1) && s.focus_ok(),
			      "text panel: Up and Down scroll a line, the page buttons a page, the wheel too, and it stops at both ends");
		}

		// Online
		online_state os;
		os.game = "NBA JAM Tournament Edition";
		os.local_ips = { "192.168.1.20", "100.84.12.7" };
		screen on = make_online(os);
		walk(on, "Online");
		CHECK(on.focused()->id == on_host && on.find(on_address)->value_text == "not set" && on.find(on_port)->value_text == "7845" && on.find(on_delay)->value == 4 &&
		      on.find(on_join)->value_text == "no address yet" && on.tabs[0].panels[0].title == os.game && text_of(on, 0, 0).find("192.168.1.20, 100.84.12.7") != std::string::npos,
		      "Online: host and join, the address, port and delay, and the local addresses to tell the others");
		for (int i = 0; i < 5; i++) on.handle(ev(act::down));
		result dl = on.handle(ev(act::right));
		CHECK(dl.what == result::changed && dl.id == on_delay && on.find(on_delay)->value == 5, "Online: the input delay changes with Left and Right");
		os.address = "203.0.113.9"; os.typing = 2; os.typed = "78"; os.available = false; os.delay = 5;
		update_online(on, os);
		CHECK(on.find(on_port)->value_text == "78_" && on.find(on_address)->value_text == "203.0.113.9" && on.find(on_join)->value_text.empty() && on.find(on_host)->dim && on.find(on_host)->value_text == "No ROMs",
		      "Online: a port being typed is shown as it is typed; a game without ROMs cannot be hosted and says so");

		// the lobby
		lobby_state ls;
		ls.game = os.game; ls.local_ips = os.local_ips;
		ls.players = { { "Player 1", "you (host)" } };
		screen lb = make_lobby(ls);
		walk(lb, "Lobby (host)");
		CHECK(lb.focused()->id == lb_start && lb.tabs[0].panels[0].title == "Players: 1 of 4" && text_of(lb, 0, 0).find("start alone") != std::string::npos &&
		      text_of(lb, 0, 2).find("UDP port: 7845") != std::string::npos, "Lobby: the host starts on Start, alone if nobody joined, and sees what to tell the others");
		ls.players.push_back({ "Player 2", "192.168.1.31  12 ms  ready" });
		ls.players.push_back({ "Player 3", "192.168.1.40  30 ms" });
		ls.delay = 6;
		update_lobby(lb, ls);
		CHECK(lb.tabs[0].panels[0].title == "Players: 3 of 4" && lb.find(lb_delay)->value == 6 && text_of(lb, 0, 0).find("Player 2   192.168.1.31  12 ms  ready") != std::string::npos && lb.focused()->id == lb_start,
		      "Lobby: players who join are listed with their ping; focus stays where it was");
		lobby_state lj;
		lj.host = false; lj.address = "203.0.113.9";
		screen jn = make_lobby(lj);
		walk(jn, "Lobby (joining)");
		bool waiting = text_of(jn, 0, 0).find("Waiting for an answer") != std::string::npos && jn.focused()->id == lb_leave && !jn.find(lb_start);
		lj.slot = 2; lj.ping = 41; lj.count = 3;
		update_lobby(jn, lj);
		CHECK(waiting && text_of(jn, 0, 0).find("You are player 3") != std::string::npos && text_of(jn, 0, 0).find("41 ms") != std::string::npos && text_of(jn, 0, 0).find("Waiting for the host to start") != std::string::npos,
		      "Lobby: a player who joins sees the answer arrive, which player they are and the ping; only the host can start");

		// About and help
		about_state as;
		as.version = "2026.11.06"; as.build = "abc1234  2026-11-17"; as.data_dir = "C:/Users/someone/Games/NBAJamRecomp"; as.latest = "v2026.12.01"; as.latest_new = true;
		as.game = "NBA Hangtime"; as.profile = "Fight stick"; as.pad = "Xbox Wireless Controller";
		static const char *an[13] = { "Up", "Down", "Left", "Right", "Shoot", "Pass", "Turbo", "Start", "Coin", "Shove", "Turbo shot", "Shortcut 1", "Shortcut 2" };
		static const char *ak[9] = { "Up", "Down", "Left", "Right", "Z", "X", "C", "1", "5" };
		for (int i = 0; i < 13; i++) as.controls.push_back({ an[i], i < 9 ? ak[i] : "", i == 9 ? "x" : i > 10 ? "-" : "a" });
		as.controls[11].action = "Shortcut 1 = pass";
		screen ab = make_about(as);
		walk(ab, "About and help");
		CHECK(ab.tabs.size() == 4 && text_of(ab, 0, 0).find("Version 2026.11.06") != std::string::npos && text_of(ab, 0, 0).find("v2026.12.01 (new: see News)") != std::string::npos &&
		      text_of(ab, 0, 0).find(as.data_dir) != std::string::npos && text_of(ab, 3, 0).find("ymfm") != std::string::npos && text_of(ab, 2, 0).find("Screenshot F12") != std::string::npos,
		      "About: the version, the latest release, where the files are, the program's keys and the credits");
		CHECK(text_of(ab, 1, 1).find("Shoot Z") != std::string::npos && text_of(ab, 1, 1).find("Shove") == std::string::npos && text_of(ab, 1, 2).find("Shove x") != std::string::npos && text_of(ab, 1, 2).find("Shortcut 1 = pass -") != std::string::npos &&
		      ab.tabs[1].panels[2].title == "Controller: Xbox Wireless Controller" && text_of(ab, 1, 0).find("Fight stick") != std::string::npos &&
		      [&] { screen c = ab; c.cur_tab = 1; return int(c.tabs[1].panels[2].items.size()) <= c.rows_visible(2) && int(c.tabs[1].panels[1].items.size()) <= c.rows_visible(1); }() &&
		      [&] { screen c = ab; c.cur_tab = 2; return int(c.tabs[2].panels[0].items.size()) <= c.rows_visible(0); }(),
		      "Help: the controls are listed from what is set now, keys and controller side by side, and fit without scrolling");
		as.controls[4].key = "Space"; as.controls[4].binding = "b, y"; as.player = 2; as.pad.clear();
		update_about(ab, as);
		CHECK(text_of(ab, 1, 1).find("Shoot Space") != std::string::npos && text_of(ab, 1, 2).find("Shoot b, y") != std::string::npos && ab.find(ab_player)->choice == 2 &&
		      ab.tabs[1].panels[2].title == "Controller: none", "Help: a changed key or binding, and another player, show at once");

		// News
		news_state ns;
		ns.ok = true; ns.is_new = true; ns.name = "NBA JAM Recomp 2026.12.01"; ns.date = "2026-12-01"; ns.version = "2026.11.06"; ns.page = "github.com/richvam/NBA-JAMs-Recomp/releases";
		ns.lines = { "ADDED", "- A Settings screen from the main menu, with picture shape, scaling, look, scanline strength and everything else that was only on a key before.", "- News.", "", "FIXED" };
		for (int i = 0; i < 30; i++) ns.lines.push_back("- Fix number " + std::to_string(i + 1) + ".");
		screen nw = make_news(ns);
		walk(nw, "News");
		const panel &np = nw.tabs[0].panels[0];
		int heads = 0, indented = 0;
		for (auto &i : np.items) { heads += i.k == kind::header; indented += i.label.rfind("  ", 0) == 0; }
		screen nd = nw;
		nd.handle(ev(act::page_down));
		CHECK(np.title == "NBA JAM Recomp 2026.12.01  2026-12-01" && np.items[0].label == "NEW VERSION - YOU HAVE 2026.11.06" && heads == 4 && indented >= 1 && text_of(nw, 0, 0).find("Fix number 30.") != std::string::npos &&
		      int(np.items.size()) > nw.rows_visible(0) && nd.tabs[0].panels[0].scroll > 0, "News: the notes with their headings, long entries wrapped, and it scrolls");
		news_state off; off.on = false;
		news_state none;
		CHECK(text_of(make_news(off), 0, 0).find("Settings") != std::string::npos && text_of(make_news(none), 0, 0).find("No news") != std::string::npos, "News: says when it is switched off, and when there is nothing to show");

		theme th2;
		as.pad = "Xbox Wireless Controller"; as.player = 0;
		screen ab2 = make_about(as), ab3 = make_about(as), ab4 = make_about(as);
		ab2.cur_tab = 1; ab2.fix_focus(); ab3.cur_tab = 2; ab3.fix_focus(); ab4.cur_tab = 3; ab4.fix_focus();
		os.typing = 0; os.available = true;
		screen shots[9] = { make_online(os), lb, jn, make_about(as), ab2, ab3, ab4, nw, nd };
		static const char *files[9] = { "online_1.png", "online_2_lobby_host.png", "online_3_lobby_join.png", "about_1.png", "about_2_controls.png", "about_3_keys.png", "about_4_credits.png", "news_1.png", "news_2_scrolled.png" };
		for (int i = 0; i < 9; i++)
		{
			canvas cv(screen::kW, screen::kH);
			shots[i].draw(cv, th2, i == 1 ? device::xbox : device::keyboard, 0);
			write_png(out, files[i], cv);
		}
	}

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
