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

// ---------------------------------------------------------------- shared by the screens below
// text wrapped to a panel's width, one label a line
static void add_text(std::vector<item> &items, const std::string &text, int px, int most = 40)
{
	for (auto &l : wrap_px(text, px, face::list, most)) items.push_back(label(l));
}

// a line that is not selected: a name at the left and what it is at the right
static item row(const std::string &name, const std::string &value)
{
	item i = label(name);
	i.value_text = value;
	return i;
}

static std::string joined(const std::vector<std::string> &v, const char *none)
{
	std::string all;
	for (auto &s : v) all += (all.empty() ? "" : ", ") + s;
	return all.empty() ? none : all;
}

// ---------------------------------------------------------------- Online (U3.5)
screen make_online(const online_state &st)
{
	screen s;
	s.title = "ONLINE GAME";
	tab t;
	t.name = "Play online";
	panel play;
	play.r = { 0.0f, 0.0f, 0.5f, 1.0f };
	play.items = { button(on_host, "Host a game"), button(on_join, "Join a game"), header("CONNECTION"), button(on_address, "Address"),
	               button(on_port, "UDP port"), slider(on_delay, "Delay", float(st.delay), 1, 10, 1) };
	panel help;
	help.title = "How it works";
	help.r = { 0.52f, 0.0f, 0.48f, 1.0f };
	t.panels = { play, help };
	s.tabs = { t };
	s.fix_focus();
	update_online(s, st);
	return s;
}

void update_online(screen &s, const online_state &st)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 2) return;
	s.tabs[0].panels[0].title = st.game;
	if (item *i = s.find(on_host)) { i->value_text = st.available ? "" : "No ROMs"; i->dim = !st.available; }
	if (item *i = s.find(on_join)) i->value_text = st.address.empty() ? "no address yet" : "";
	if (item *i = s.find(on_address)) i->value_text = st.typing == 1 ? st.typed + "_" : st.address.empty() ? "not set" : st.address;
	if (item *i = s.find(on_port)) i->value_text = st.typing == 2 ? st.typed + "_" : std::to_string(st.port);
	if (item *i = s.find(on_delay)) i->value = float(std::clamp(st.delay, 1, 10));
	panel &help = s.tabs[0].panels[1];
	const int px = int(s.panel_rect(1).w) - 30;
	std::vector<item> items;
	add_text(items, "The host picks the game and plays as player 1. Everybody needs the same ROMs.", px);
	items.push_back(header("THE HOST"));
	add_text(items, "Opens the UDP port on the router, or everybody uses a virtual LAN (Tailscale, ZeroTier, Radmin VPN).", px);
	items.push_back(header("INPUT DELAY"));
	add_text(items, "3 or 4 on a LAN, 5 to 7 over the internet. The host decides.", px);
	help.items = items;
	// under the connection: this computer's own address, for the host to tell the others
	panel &play = s.tabs[0].panels[0];
	play.items.resize(6);
	play.items.push_back(header("YOUR LOCAL ADDRESS"));
	add_text(play.items, joined(st.local_ips, "not known"), int(s.panel_rect(0).w) - 30, 4);
	s.fix_focus();
}

screen make_lobby(const lobby_state &st)
{
	screen s;
	s.title = "ONLINE GAME";
	tab t;
	t.name = st.host ? "Hosting" : "Joining";
	panel who;
	who.r = { 0.0f, 0.0f, 1.0f, 0.38f };
	panel acts;
	acts.r = { 0.0f, 0.4f, 0.5f, 0.6f };
	if (st.host) acts.items = { slider(lb_delay, "Delay", float(st.delay), 1, 10, 1), button(lb_start, "Start the game"), button(lb_leave, "Close the game") };
	else acts.items = { button(lb_leave, "Leave") };
	panel info;
	info.title = "This game";
	info.r = { 0.52f, 0.4f, 0.48f, 0.6f };
	t.panels = { who, acts, info };
	s.tabs = { t };
	s.cur_panel = 1;
	if (st.host) s.tabs[0].panels[1].focus = 1;         // on Start
	s.fix_focus();
	update_lobby(s, st);
	return s;
}

void update_lobby(screen &s, const lobby_state &st)
{
	if (s.tabs.empty() || s.tabs[0].panels.size() < 3) return;
	panel &who = s.tabs[0].panels[0], &info = s.tabs[0].panels[2];
	const int px = int(s.panel_rect(2).w) - 30;
	std::vector<item> w, i;
	if (st.host)
	{
		who.title = "Players: " + std::to_string(st.players.size()) + " of 4";
		for (auto &p : st.players) w.push_back(label(p.name + "   " + p.status));
		if (st.players.size() < 2) { w.push_back(label("")); add_text(w, "Nobody has joined yet. You can start alone to try it.", int(s.panel_rect(0).w) - 30, 3); }
		if (item *d = s.find(lb_delay)) d->value = float(std::clamp(st.delay, 1, 10));
		i.push_back(header("GAME"));
		add_text(i, st.game, px, 2);
		i.push_back(header("TELL THE OTHERS"));
		add_text(i, "Address: " + joined(st.local_ips, "not known"), px, 3);
		i.push_back(label("UDP port: " + std::to_string(st.port)));
		add_text(i, "Over the internet: your public or virtual LAN address.", px, 3);
	}
	else
	{
		who.title = "Connection";
		w.push_back(row("Host", st.address));
		w.push_back(row("UDP port", std::to_string(st.port)));
		if (st.slot < 0) w.push_back(label("Waiting for an answer..."));
		else
		{
			w.push_back(row("You are", "player " + std::to_string(st.slot + 1)));
			w.push_back(row("Ping", std::to_string(st.ping) + " ms"));
			w.push_back(row("Players", std::to_string(st.count)));
			w.push_back(label(st.receiving ? "Receiving the game data..." : "Waiting for the host to start..."));
		}
		i.push_back(header("GAME"));
		add_text(i, "The host picks the game. It starts here when the host starts it.", px, 4);
		i.push_back(header("YOUR CONTROLS"));
		add_text(i, "The player 1 keys, or the first controller.", px, 3);
	}
	who.items = w;
	info.items = i;
	s.fix_focus();
}

// ---------------------------------------------------------------- About and help (U3.6)
screen make_about(const about_state &st)
{
	screen s;
	s.title = "ABOUT AND HELP";
	const int px = screen::kW - 16 - 30;

	tab about;
	about.name = "About";
	panel a;
	a.r = { 0.0f, 0.0f, 1.0f, 1.0f };
	a.items.push_back(row("Version", st.version));
	a.items.push_back(row("Build", st.build));
	if (!st.latest.empty()) a.items.push_back(label("Latest release: " + st.latest + (st.latest_new ? " (new: see News)" : " (you are up to date)")));
	a.items.push_back(header(st.portable ? "YOUR FILES (PORTABLE)" : "YOUR FILES"));
	add_text(a.items, st.data_dir, px, 3);
	a.items.push_back(header("WHAT THIS IS"));
	add_text(a.items, "A recompilation of Midway's NBA JAM saga from the original source code that was made public.", px);
	add_text(a.items, "Created by Pocket_lucho with Claude (Anthropic).", px);
	a.items.push_back(header("BUILT FROM SOURCE"));
	a.items.push_back(label("T.E. rev 4.0 (identical to the final ROM)"));
	a.items.push_back(label("T.E. Special Edition rev 5.02 (identical)"));
	a.items.push_back(label("Maximum Hangtime 1.3 (identical to the ROM)"));
	about.panels = { a };

	tab ctl;
	ctl.name = "Controls";
	panel who;
	who.r = { 0.0f, 0.0f, 1.0f, 0.19f };
	who.items = { picker(ab_player, "Player", { "Player 1", "Player 2", "Player 3", "Player 4" }, st.player), label("") };
	panel keys;
	keys.title = "Keyboard";
	keys.r = { 0.0f, 0.21f, 0.4f, 0.79f };
	panel pad;
	pad.r = { 0.42f, 0.21f, 0.58f, 0.79f };
	ctl.panels = { who, keys, pad };

	tab prog;
	prog.name = "Keys";
	panel k;
	k.title = "The program's keys";
	k.r = { 0.0f, 0.0f, 1.0f, 1.0f };
	static const char *const kKeys[][2] = {
		{ "Back to the menu, leave a game", "Esc" }, { "Pause", "P" }, { "Help over the game", "F1" }, { "TEST switch (service menu)", "F2" },
		{ "Reset the game", "F3" }, { "Service credit", "9" }, { "Picture look", "F4" }, { "Scaling", "F5" },
		{ "Scanline strength", "F6" }, { "Picture shape", "F7" }, { "Sides of a 4:3 picture", "F8" },
		{ "Fullscreen", "F11 or Alt+Enter" }, { "Screenshot", "F12 or Print Screen" }, { "Volume", "- and +" }, { "Online game, from the menu", "N" },
	};
	for (auto &r : kKeys) k.items.push_back(row(r[0], r[1]));
	add_text(k.items, "F4 to F8, F11 and the volume are also in Settings.", px, 2);
	prog.panels = { k };

	tab cred;
	cred.name = "Credits";
	panel c;
	c.r = { 0.0f, 0.0f, 1.0f, 1.0f };
	c.items.push_back(header("TOOLS"));
	add_text(c.items, "Own TI GSPA assembler and Midway's LOAD2 (emu2).", px);
	c.items.push_back(header("EMULATION"));
	add_text(c.items, "MAME cores (TMS34010, 6809, ADSP-2105) and ymfm.", px);
	c.items.push_back(header("SOURCES"));
	c.items.push_back(label("github.com/historicalsource and Asure"));
	c.items.push_back(label("github.com/PocketLucho/NBA-JAM---Recompiled"));
	c.items.push_back(header("RELEASES"));
	c.items.push_back(label("github.com/richvam/NBA-JAMs-Recomp"));
	c.items.push_back(header("IF YOU WANT TO SUPPORT POCKET_LUCHO"));
	c.items.push_back(label("buymeacoffee.com/pocket_lucho"));
	c.items.push_back(label(""));
	c.items.push_back(label("NBA JAM (c) Midway. Non-profit fan project."));
	cred.panels = { c };

	s.tabs = { about, ctl, prog, cred };
	s.fix_focus();
	update_about(s, st);
	return s;
}

void update_about(screen &s, const about_state &st)
{
	if (s.tabs.size() < 2 || s.tabs[1].panels.size() < 3) return;
	if (item *i = s.find(ab_player)) i->choice = std::clamp(st.player, 0, 3);
	panel &keys = s.tabs[1].panels[1], &pad = s.tabs[1].panels[2];
	pad.title = st.pad.empty() ? "Controller: none" : "Controller: " + st.pad;
	keys.items.clear();
	pad.items.clear();
	for (auto &r : st.controls)
	{
		if (!r.key.empty()) keys.items.push_back(row(r.action, r.key));
		pad.items.push_back(row(r.action, r.binding));
	}
	const int px = int((screen::kW - 16) * 0.4f) - 30;       // the width of the keyboard's list
	keys.items.push_back(label(""));
	add_text(keys.items, "The keys are in the settings file, nbajam_recomp.ini.", px, 3);
	// which profile that is, under the player
	panel &who = s.tabs[1].panels[0];
	if (who.items.size() > 1) who.items[1] = row("Profile, set in Controls", st.profile);
}

// ---------------------------------------------------------------- News
screen make_news(const news_state &st)
{
	screen s;
	s.title = "NEWS";
	tab t;
	t.name = "Latest release";
	panel p;
	p.r = { 0.0f, 0.0f, 1.0f, 1.0f };
	const int px = screen::kW - 16 - 30;
	if (!st.on) add_text(p.items, "News is switched off. Switch on \"Check for new versions\" in Settings, on the Program tab, to see the latest release here.", px);
	else if (!st.ok) add_text(p.items, "No news. Either there is no connection, or no version has been published yet.", px);
	else
	{
		p.title = st.name + (st.date.empty() ? "" : "  " + st.date);
		p.items.push_back(header(st.is_new ? "NEW VERSION - YOU HAVE " + st.version : "YOU HAVE " + st.version + ": UP TO DATE"));
		for (const std::string &l : st.lines)
		{
			if (l.empty()) { p.items.push_back(label("")); continue; }
			bool caps = l[0] != '-' && l[0] != ' ', letter = false;
			for (char c : l) { caps = caps && !(c >= 'a' && c <= 'z'); letter = letter || (c >= 'A' && c <= 'Z'); }
			if (caps && letter) { p.items.push_back(header(fit_px(l, px, face::list))); continue; }
			// a list entry's further lines are set in under its first
			const bool entry = l.rfind("- ", 0) == 0;
			auto w = wrap_px(l, entry ? px - 12 : px, face::list, 12);
			for (size_t k = 0; k < w.size(); k++) p.items.push_back(label(k && entry ? "  " + w[k] : w[k]));
		}
		if (!st.page.empty()) { p.items.push_back(label("")); p.items.push_back(header("THE RELEASES PAGE")); add_text(p.items, st.page, px, 2); }
	}
	t.panels = { p };
	s.tabs = { t };
	s.fix_focus();
	return s;
}

} // namespace ui
