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

// ---------------------------------------------------------------- Lua mods (U3.4)
screen make_mods(const mods_state &st)
{
	screen s;
	s.title = "LUA MODS";
	s.prompts = { { act::context, "On/off" } };
	tab t;
	t.name = "Scripts";
	panel list;
	list.title = "Scripts";
	list.r = { 0.0f, 0.0f, 0.4f, 1.0f };
	for (size_t i = 0; i < st.mods.size(); i++) list.items.push_back(button(md_mod0 + int(i), st.mods[i].name));
	if (st.mods.empty()) list.items.push_back(label("No scripts found"));
	panel page;
	page.r = { 0.42f, 0.0f, 0.58f, 1.0f };
	t.panels = { list, page };
	s.tabs = { t };
	s.fix_focus();
	update_mods(s, st, 0);
	return s;
}

int mods_index(const screen &s, int last)
{
	const item *f = s.focused();
	if (s.cur_panel == 0 && f && f->id >= md_mod0 && f->id < md_enabled) return f->id - md_mod0;
	return last;
}

void update_mods(screen &s, const mods_state &st, int mod)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 2) return;
	for (size_t i = 0; i < st.mods.size(); i++)
		if (item *it = s.find(md_mod0 + int(i)))
		{
			const mod_view &m = st.mods[i];
			it->value_text = !m.error.empty() ? "Error" : m.enabled ? "On" : "Off";
			it->dim = !m.enabled || !m.error.empty();
		}
	panel &page = s.tabs[0].panels[1];
	const int px = int(s.panel_rect(1).w) - 30;
	std::vector<item> items;
	auto lines = [&](const std::string &text, int most) { for (auto &l : wrap_px(text, px, face::list, most)) items.push_back(label(l)); };
	if (st.mods.empty())
	{
		lines("Put .lua scripts in this folder:", 2);
		lines(st.folder, 4);
		lines("They are listed here the next time this screen opens.", 3);
	}
	else
	{
		const mod_view &m = st.mods[size_t(std::clamp(mod, 0, int(st.mods.size()) - 1))];
		page.title = m.name;
		if (!m.error.empty())
		{
			items.push_back(header("THIS SCRIPT HAS AN ERROR"));
			lines(m.error, 8);
		}
		else
		{
			lines(m.description, 5);
			items.push_back(toggle(md_enabled, "Mod enabled", m.enabled));
			items.push_back(picker(md_apply, "Apply to", { "All games", "Selected games" }, m.all_games ? 0 : 1));
			if (!m.options.empty()) items.push_back(header("OPTIONS"));
			for (size_t i = 0; i < m.options.size(); i++) items.push_back(picker(md_opt0 + int(i), m.options[i].label, m.options[i].values, m.options[i].value));
			items.push_back(header("GAMES"));
			for (size_t i = 0; i < m.games.size(); i++)
			{
				item g = toggle(md_game0 + int(i), m.games[i].supported ? m.games[i].title : m.games[i].title + " (not supported)", m.games[i].on);
				g.disabled = !m.games[i].supported;
				items.push_back(g);
			}
		}
	}
	if (st.mods.empty()) page.title.clear();
	page.items = items;
	if (page.focus >= int(page.items.size())) page.focus = 0;
	s.fix_focus();
}

bool mods_open_page(screen &s)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 2 || s.cur_panel != 0) return false;
	panel &page = s.tabs[0].panels[1];
	for (size_t i = 0; i < page.items.size(); i++)
		if (page.items[i].focusable()) { s.cur_panel = 1; page.focus = int(i); page.scroll = 0; return true; }
	return false;
}

// ---------------------------------------------------------------- Controls (U3.3)
screen make_controls(const controls_state &st)
{
	screen s;
	s.title = "CONTROLS";
	s.prompts = { { act::context, "Clear" } };
	tab t;
	t.name = "Controller";
	panel who;
	who.title = "Who";
	who.r = { 0.0f, 0.0f, 1.0f, 0.23f };
	who.items = { picker(ct_game, "Game", st.games, st.game), picker(ct_player, "Player", { "Player 1", "Player 2", "Player 3", "Player 4" }, st.player),
	              picker(ct_profile, "Profile", st.profiles, st.profile) };
	panel acts;
		acts.r = { 0.0f, 0.25f, 0.52f, 0.75f };
	for (size_t i = 0; i < st.actions.size(); i++) acts.items.push_back(button(ct_act0 + int(i), st.actions[i].name));
	panel prof;
	prof.title = "This profile";
	prof.r = { 0.54f, 0.25f, 0.46f, 0.75f };
	prof.items = {
		picker(ct_short0, "Shortcut 1", st.shortcut_sets, st.shortcut[0]), picker(ct_short0 + 1, "Shortcut 2", st.shortcut_sets, st.shortcut[1]),
		slider(ct_dead, "Dead zone", float(st.deadzone), 5, 95, 5),
		button(ct_new, "New profile (a copy)"), button(ct_reset, "Reset to defaults"), button(ct_delete, "Delete profile"),
		button(ct_export, "Export profile"), button(ct_export_setup, "Export setup"), button(ct_import, "Import"),
		label("* held now"),
	};
	t.panels = { who, acts, prof };
	s.tabs = { t };
	s.fix_focus();
	update_controls(s, st);
	return s;
}

void update_controls(screen &s, const controls_state &st)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 3) return;
	auto pick = [&](int id, const std::vector<std::string> &choices, int v)
	{
		if (item *i = s.find(id)) { i->choices = choices; i->choice = std::clamp(v, 0, std::max(0, int(choices.size()) - 1)); }
	};
	pick(ct_game, st.games, st.game);
	if (item *i = s.find(ct_player)) i->choice = std::clamp(st.player, 0, 3);
	pick(ct_profile, st.profiles, st.profile);
	pick(ct_short0, st.shortcut_sets, st.shortcut[0]);
	pick(ct_short0 + 1, st.shortcut_sets, st.shortcut[1]);
	if (item *i = s.find(ct_dead)) i->value = float(std::clamp(st.deadzone, 5, 95));
	// whose buttons: the player's controller, in the panel's title
	s.tabs[0].panels[1].title = st.pad.empty() ? "Buttons (no controller)" : "Buttons: " + st.pad;
	for (size_t a = 0; a < st.actions.size(); a++)
		if (item *i = s.find(ct_act0 + int(a)))
		{
			if (st.capturing == int(a)) i->value_text = "Press it... " + std::to_string(st.seconds);
			else i->value_text = (st.actions[a].held ? "* " : "") + st.actions[a].binding;
		}
	if (item *i = s.find(ct_new)) i->label = st.naming ? "Name: " + st.name + "_" : "New profile (a copy)";
	if (item *i = s.find(ct_export_setup)) i->label = st.game > 0 ? "Export this game's setup" : "Export all-games setup";
}

} // namespace ui
