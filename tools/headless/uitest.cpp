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
#include "miniz.h"
#include <cstdio>
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

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
