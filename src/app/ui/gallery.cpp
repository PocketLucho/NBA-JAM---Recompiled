// gallery.cpp - see gallery.h
#include "gallery.h"

namespace ui
{

screen make_gallery(const std::vector<std::string> &themes)
{
	screen s;
	s.title = "WIDGET GALLERY";
	s.prompts = { { act::context, "Details" } };

	// tab 1: a list with group headers beside a panel of settings
	tab t1;
	t1.name = "Widgets";
	panel list;
	list.title = "List";
	list.r = { 0.0f, 0.0f, 0.48f, 1.0f };
	list.items = {
		header("PLAY"),
		button(g_play, "Start a game", "T.E. 4.0"),
		button(g_ask, "Ask me something"),
		button(g_toast, "Show a toast"),
		header("NOT AVAILABLE"),
		[] { item i = button(90, "Greyed out button", "no ROM"); i.disabled = true; return i; }(),
		header("LONG TEXT"),
		button(91, "A label that is far too long for its row and scrolls while it has the focus"),
		label("Plain text line"),
	};
	panel set;
	set.title = "Settings";
	set.r = { 0.5f, 0.0f, 0.5f, 0.62f };
	set.items = {
		toggle(g_sound, "Menu sounds", true),
		toggle(g_music, "Menu music", false),
		picker(g_shader, "Picture", { "Sharp", "Smooth", "Scanlines", "CRT", "Curved CRT" }, 3),
		slider(g_volume, "Volume", 0.8f, 0.0f, 1.0f, 0.1f),
		slider(g_delay, "Input delay", 4, 1, 9, 1),
		[] { item i = toggle(92, "Locked online", true); i.disabled = true; return i; }(),
	};
	panel more;
	more.title = "Below";
	more.r = { 0.5f, 0.66f, 0.5f, 0.34f };
	more.items = { button(93, "Reached with Down"), button(94, "Second button") };
	t1.panels = { list, set, more };

	// tab 2: a button grid over a row of plain buttons
	tab t2;
	t2.name = "Grid";
	panel grid;
	grid.title = "Button grid";
	grid.r = { 0.0f, 0.0f, 1.0f, 0.5f };
	grid.cols = 4;
	static const char *names[12] = { "Q", "W", "E", "R", "A", "S", "D", "F", "Z", "X", "C", "V" };
	for (int i = 0; i < 12; i++)
	{
		item b = button(g_grid0 + i, names[i]);
		if (i == 5) b.disabled = true;
		grid.items.push_back(b);
	}
	panel under;
	under.title = "Under the grid";
	under.r = { 0.0f, 0.55f, 1.0f, 0.3f };
	under.items = { button(95, "Done"), button(96, "Cancel") };
	t2.panels = { grid, under };

	// tab 3: a long list with group headers, as the team lists will be
	tab t3;
	t3.name = "Long list";
	panel longp;
	longp.title = "Forty items in five groups";
	longp.r = { 0.1f, 0.0f, 0.8f, 1.0f };
	for (int g = 0; g < 5; g++)
	{
		longp.items.push_back(header("GROUP " + std::to_string(g + 1)));
		for (int i = 0; i < 8; i++)
		{
			item b = button(g_list0 + g * 8 + i, "Item " + std::to_string(g * 8 + i + 1));
			if (g == 2 && i == 3) b.disabled = true;
			longp.items.push_back(b);
		}
	}
	t3.panels = { longp };

	// tab 4: nothing to select; focus has nowhere to go and must not break
	tab t4;
	t4.name = "Text only";
	panel txt;
	txt.title = "About this screen";
	txt.r = { 0.1f, 0.1f, 0.8f, 0.5f };
	txt.items = { label("Nothing here can be selected."), label("Shoulder buttons change tab."), label("Back leaves the gallery.") };
	t4.panels = { txt };

	s.tabs = { t1, t2, t3, t4 };
	if (!themes.empty())
	{
		// tab 5: the theme of each game family; the caller repaints the gallery in it
		tab t5;
		t5.name = "Themes";
		panel pick;
		pick.title = "Theme of each game";
		pick.r = { 0.1f, 0.0f, 0.8f, 0.7f };
		for (size_t i = 0; i < themes.size(); i++) pick.items.push_back(button(g_theme0 + int(i), themes[i]));
		pick.items.push_back(label("A game without ROMs uses the neutral theme."));
		t5.panels = { pick };
		s.tabs.push_back(t5);
	}
	s.fix_focus();
	return s;
}

bool gallery_react(screen &s, const result &r, unsigned now)
{
	switch (r.what)
	{
	case result::back: return true;
	case result::activated:
		if (r.id == g_ask) s.ask("Reset the game? Anything not saved in the game's own memory is lost.", g_ask);
		else if (r.id == g_toast) s.toast("Saved to nbajam_recomp.ini", now);
		else if (r.id >= g_theme0 && r.id < g_theme0 + 50) {}        // the caller changes theme
		else s.toast("Pressed: " + (s.find(r.id) ? s.find(r.id)->label : std::string("?")), now);
		break;
	case result::changed:
		if (item *i = s.find(r.id))
			s.toast(i->label + (i->k == kind::toggle ? (i->on ? ": on" : ": off") : i->k == kind::picker ? ": " + i->choices[size_t(i->choice)] : ": changed"), now, 1200);
		break;
	case result::context: s.toast("Details of item " + std::to_string(r.id), now); break;
	case result::yes: s.toast("You said yes", now); break;
	case result::no: s.toast("You said no", now); break;
	default: break;
	}
	return false;
}

} // namespace ui
