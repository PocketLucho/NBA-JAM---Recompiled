// screens.cpp - see screens.h
#include "screens.h"
#include <algorithm>
#include <cmath>

namespace ui
{

// ---------------------------------------------------------------- game picker (U3.1)
static const int kNameRows = 5;          // rows kept free at the top of the side panel for the large name

screen make_picker(const picker_state &st)
{
	screen s;
	s.title = "NBA JAM RECOMP";
	s.prompts = { { act::context, "Online" } };
	tab t;
	t.name = "Games";
	panel games;
	games.title = "Choose a game";
	games.r = { 0.0f, 0.0f, 0.5f, 0.45f };
	for (size_t i = 0; i < st.games.size(); i++) games.items.push_back(button(pk_game0 + int(i), st.games[i].title));
	panel menu;
	menu.title = "Menu";
	menu.r = { 0.0f, 0.48f, 0.5f, 0.52f };
	menu.items = { button(pk_online, "Online game"), button(pk_settings, "Settings"), button(pk_mods, "Lua mods"), button(pk_controls, "Controls"),
	               button(pk_news, "News"), button(pk_about, "About"), button(pk_rescan, "Look for ROMs again"), button(pk_quit, "Quit") };
	panel info;
	info.r = { 0.52f, 0.0f, 0.48f, 1.0f };
	t.panels = { games, menu, info };
	s.tabs = { t };
	s.fix_focus();
	update_picker(s, st, 0);
	return s;
}

int picker_game_index(const screen &s, int last)
{
	const item *f = s.focused();
	if (s.cur_panel == 0 && f && f->id >= pk_game0 && f->id < pk_online) return f->id - pk_game0;
	return last;
}

void update_picker(screen &s, const picker_state &st, int game)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 3) return;
	for (size_t i = 0; i < st.games.size(); i++)
		if (item *it = s.find(pk_game0 + int(i)))
		{
			it->value_text = st.games[i].available ? "" : "No ROMs";
			it->dim = !st.games[i].available;
		}
	game = std::clamp(game, 0, std::max(0, int(st.games.size()) - 1));
	const bool have = !st.games.empty();
	bool any = false;
	for (auto &g : st.games) any = any || g.available;
	if (item *it = s.find(pk_online)) { it->value_text = have ? st.games[size_t(game)].title : ""; it->disabled = !have || !st.games[size_t(game)].available; }
	if (item *it = s.find(pk_mods)) it->value_text = std::to_string(st.mods_on) + " of " + std::to_string(st.mods_total) + " on";
	if (item *it = s.find(pk_news)) it->value_text = !st.news_on ? "Off" : st.news_new ? "New version" : "";
	s.fix_focus();

	// the side panel: room for the large name, then what the game is and whether it can be played
	panel &info = s.tabs[0].panels[2];
	const int px = int(s.panel_rect(2).w) - 30;
	info.items.clear();
	auto lines = [&](const std::string &text) { for (auto &l : wrap_px(text, px, face::list, 6)) info.items.push_back(label(l)); };
	for (int i = 0; i < kNameRows; i++) info.items.push_back(label(""));
	if (have)
	{
		const picker_game &g = st.games[size_t(game)];
		lines(g.revision);
		lines("Board: " + g.board);
		if (g.from_source) lines("Builds from the public source");
		if (!any)
		{
			// first run: nothing can be played yet
			info.items.push_back(header("NO GAMES FOUND"));
			lines("Put your ROM sets (ZIP files) in this folder:");
			lines(st.roms_dir);
			lines("Then choose Look for ROMs again.");
		}
		else if (g.available) info.items.push_back(header("READY TO PLAY"));
		else
		{
			info.items.push_back(header("CANNOT BE PLAYED YET"));
			lines(g.why);
		}
	}
	if (st.news_on && st.news_new) { info.items.push_back(header("NEW VERSION AVAILABLE")); lines(st.news_tag + ": see News"); }
	// the version on the last row
	const int rows = s.rows_visible(2);
	while (int(info.items.size()) < rows - 1) info.items.push_back(label(""));
	if (int(info.items.size()) < rows) info.items.push_back(label("Version " + st.version));
}

void draw_picker_name(canvas &cv, const screen &s, const theme &th, const picker_game &g, const rom_art::art_set *art)
{
	if (g.big.empty()) return;
	const rectf r = s.panel_rect(2);
	const int room_w = int(r.w) - 12, room_h = kNameRows * screen::kRow - 6;
	const int n = int(g.big.size());
	// the tallest font of the game whose lines all fit
	const rom_art::font *best = nullptr;
	if (art)
		for (const rom_art::font &f : art->fonts)
		{
			bool fits = n * (f.cap_h + 3) <= room_h + 3;
			for (auto &l : g.big) fits = fits && text_w(l, f) <= room_w;
			if (fits && (!best || f.cap_h > best->cap_h)) best = &f;
		}
	const int line_h = best ? best->cap_h : text_h(face::title);
	int y = int(r.y) + 4 + (room_h - (n * line_h + (n - 1) * 3)) / 2;
	for (auto &l : g.big)
	{
		if (best) draw_text(cv, int(r.x + r.w / 2) - text_w(l, *best) / 2, y, l, *best, g.available ? 0xffffffff : 0xff707070);
		else
		{
			std::string t = fit_px(l, room_w, face::title);
			ink k = g.available ? ink{ th.title_top, th.title_bottom, th.outline } : th.ink_for(th.text_off);
			draw_text(cv, int(r.x + r.w / 2) - text_w(t, face::title) / 2, y, t, face::title, k);
		}
		y += line_h + 3;
	}
}

// ---------------------------------------------------------------- settings (U3.2)
screen make_settings(const settings_state &st)
{
	screen s;
	s.title = "SETTINGS";
	tab video;
	video.name = "Video";
	panel v;
	v.title = "Picture";
	v.r = { 0.1f, 0.0f, 0.8f, 0.62f };
	v.items = {
		picker(st_shape, "Shape", { "Original 4:3", "Stretch" }, st.shape),
		picker(st_scaling, "Scaling", { "Integer", "Fit", "Smooth" }, st.scaling),
		picker(st_shader, "Look", st.shaders, st.shader),
		slider(st_scanlines, "Scanlines", st.scanlines, 0.2f, 1.0f, 0.2f),
		picker(st_sides, "Sides of a 4:3 picture", st.side_names, st.sides),
		toggle(st_fullscreen, "Fullscreen", st.fullscreen),
	};
	panel vk;
	vk.r = { 0.1f, 0.66f, 0.8f, 0.3f };
	vk.items = { label("Also on keys: F7 shape, F5 scaling,"), label("F4 look, F6 scanlines, F8 sides,"), label("F11 fullscreen.") };
	video.panels = { v, vk };

	tab sound;
	sound.name = "Sound";
	panel so;
	so.title = "Sound";
	so.r = { 0.1f, 0.0f, 0.8f, 0.3f };
	so.items = { slider(st_volume, "Volume", st.volume, 0.0f, 1.0f, 0.1f), label("Also on keys: - and +.") };
	sound.panels = { so };

	tab game;
	game.name = "Game";
	panel g;
	g.title = "Cabinet switches";
	g.r = { 0.1f, 0.0f, 0.8f, 0.5f };
	g.items = {
		toggle(st_free_play, "Free play", st.free_play),
		toggle(st_players, "Four players", st.four_players),
		picker(st_country, "Country", { "USA", "French", "German" }, st.country),
		toggle(st_clips, "Video clips", st.video_clips),
		label("Used the next time a game starts."),
	};
	game.panels = { g };

	tab prog;
	prog.name = "Program";
	panel p;
	p.title = "Program";
	p.r = { 0.1f, 0.0f, 0.8f, 0.3f };
	p.items = { toggle(st_news, "Check for new versions", st.news), label("Asks the Releases page once at start."), label("Nothing is downloaded or installed.") };
	prog.panels = { p };

	s.tabs = { video, sound, game, prog };
	s.fix_focus();
	return s;
}

void show_settings(screen &s, const settings_state &st)
{
	auto pick = [&](int id, int v) { if (item *i = s.find(id)) i->choice = std::clamp(v, 0, std::max(0, int(i->choices.size()) - 1)); };
	auto tog = [&](int id, bool v) { if (item *i = s.find(id)) i->on = v; };
	auto sli = [&](int id, float v) { if (item *i = s.find(id)) i->value = std::clamp(v, i->min, i->max); };
	if (item *i = s.find(st_sides)) i->choices = st.side_names;
	pick(st_shape, st.shape); pick(st_scaling, st.scaling); pick(st_shader, st.shader); pick(st_sides, st.sides); pick(st_country, st.country);
	tog(st_fullscreen, st.fullscreen); tog(st_free_play, st.free_play); tog(st_players, st.four_players); tog(st_clips, st.video_clips); tog(st_news, st.news);
	sli(st_scanlines, st.scanlines); sli(st_volume, st.volume);
}

void read_settings(const screen &s, settings_state &st)
{
	screen &m = const_cast<screen &>(s);
	auto pick = [&](int id, int &v) { if (const item *i = m.find(id)) v = i->choice; };
	auto tog = [&](int id, bool &v) { if (const item *i = m.find(id)) v = i->on; };
	auto sli = [&](int id, float &v) { if (const item *i = m.find(id)) v = std::round(i->value * 10.0f) / 10.0f; };
	pick(st_shape, st.shape); pick(st_scaling, st.scaling); pick(st_shader, st.shader); pick(st_sides, st.sides); pick(st_country, st.country);
	tog(st_fullscreen, st.fullscreen); tog(st_free_play, st.free_play); tog(st_players, st.four_players); tog(st_clips, st.video_clips); tog(st_news, st.news);
	sli(st_scanlines, st.scanlines); sli(st_volume, st.volume);
}

} // namespace ui
