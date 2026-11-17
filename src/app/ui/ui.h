// ui.h - the menu toolkit (roadmap Part 2, phase U1). No SDL in here: the app feeds it
// events through ui_sdl.h, the headless tests feed it by hand.
//
// A screen is: a title, tabs along the top, panels with a title strip inside each tab, a
// status line of button prompts along the bottom. A panel holds items: group headers,
// buttons (one column or a grid), toggles, value pickers and sliders. On top of that a
// screen can show one confirm dialog and one toast message.
//
// Positions are in canvas units (400 x 300, shown 4:3 at any window size by the presenter),
// so a layout holds from 1024x768 to 5120x1440 without change.
//
// THE NAVIGATION RULE (U1.2), one rule for every screen:
//   Up / Down      next item in the panel that can take focus (headers and disabled items
//                  are skipped). At the top or bottom edge: the panel above or below, if
//                  there is one; otherwise focus stays.
//   Left / Right   on a toggle, picker or slider: change its value.
//                  on a button: the next button in a grid row; at the edge of the row, or in
//                  a plain list, the panel to that side, if there is one; otherwise stays.
//   Shoulders      previous / next tab (wraps round). Focus goes to the first usable item.
//   Confirm        press the button, flip the toggle, step the picker.
//   Back           always reported to the caller, from any state; in a dialog it means No.
//   Page up/down   one panel-height of items.
//   Focus is never lost and never rests on a header or a disabled item.
#pragma once
#include "canvas.h"
#include "lettering.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace ui
{

// ---------------------------------------------------------------- input (U1.3)
enum class act { none, up, down, left, right, confirm, back, tab_prev, tab_next, page_up, page_down, context };
const char *act_name(act a);

// which kind of device was used last: decides the button prompts (U1.4)
enum class device { keyboard, xbox, playstation, nintendo, mouse, touch };

// keys and pad buttons the menus understand. The mapping is fixed: the in-game control
// profiles never apply to menus, so a bad remap cannot lock anyone out.
enum class key { up, down, left, right, enter, space, escape, backspace, tab, page_up, page_down, q, e, del, other };
enum class pad { up, down, left, right, south, east, west, north, l1, r1, l2, r2, start, select, other };

struct raw_event
{
	enum kind_t { key_down, key_up, pad_down, pad_up, pad_stick, mouse_move, mouse_down, mouse_up, wheel, touch_down, touch_move, touch_up } kind = key_down;
	key k = key::other;
	pad p = pad::other;
	device dev = device::keyboard;   // for pad events: xbox, playstation or nintendo
	float x = 0, y = 0;              // pointer position in canvas units; pad_stick: left stick, -1..1; wheel: y = notches
	bool shift = false;              // key_down of tab: shift = previous tab
};

struct event
{
	act a = act::none;
	enum ptr_t { p_none, hover, click, scroll } ptr = p_none;
	float x = 0, y = 0;              // hover / click / scroll position in canvas units
	float amount = 0;                // scroll: rows, positive = further down the list
};

// Turns raw events into menu events, with hold-to-repeat that speeds up in long lists.
class input
{
public:
	void feed(const raw_event &e, unsigned now_ms);
	void tick(unsigned now_ms);              // call every frame: produces the repeats
	bool next(event &out);
	device last_device() const { return m_dev; }
	// first repeat after this long, then faster and faster down to the minimum
	static constexpr unsigned kRepeatDelay = 400, kRepeatStart = 130, kRepeatMin = 35;

private:
	void push(act a) { event e; e.a = a; m_q.push_back(e); }
	void hold(act a, unsigned now);
	void release(act a);
	std::deque<event> m_q;
	device m_dev = device::keyboard;
	act m_held = act::none;
	unsigned m_next = 0, m_interval = 0;
	int m_stick_x = 0, m_stick_y = 0;        // -1, 0, 1 after the dead zone
	bool m_mouse_down = false, m_touch = false, m_dragged = false;
	float m_down_x = 0, m_down_y = 0, m_last_y = 0;
};

// What to print for an action on a device: "A", "Cross", "Enter"... (U1.4)
std::string glyph(device d, act a);

// ---------------------------------------------------------------- text (U1.5)
// At most max_chars characters; a longer text ends in "..", never cut mid-letter by the edge.
std::string fit(const std::string &s, int max_chars);
// The focused row scrolls a text that does not fit instead: a window of max_chars
// characters that moves with time, pausing at both ends.
std::string marquee(const std::string &s, int max_chars, unsigned now_ms);

// ---------------------------------------------------------------- widgets (U1.1)
struct rectf { float x = 0, y = 0, w = 0, h = 0; bool has(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

enum class kind { header, button, toggle, picker, slider, label };

struct item
{
	kind k = kind::button;
	int id = 0;
	std::string label;
	bool disabled = false;
	bool dim = false;                      // drawn greyed, but can be highlighted and pressed (a game without ROMs)
	bool on = false;                       // toggle
	std::vector<std::string> choices;      // picker
	int choice = 0;
	float value = 0, min = 0, max = 1, step = 0.1f;   // slider
	std::string value_text;                // button: text at the right (a current setting, a status)
	bool focusable() const { return !disabled && k != kind::header && k != kind::label; }
};

item header(const std::string &label);
item label(const std::string &text);
item button(int id, const std::string &label, const std::string &value_text = "");
item toggle(int id, const std::string &label, bool on);
item picker(int id, const std::string &label, const std::vector<std::string> &choices, int choice);
item slider(int id, const std::string &label, float value, float min, float max, float step);

struct panel
{
	std::string title;
	rectf r;                    // in fractions (0..1) of the area between the tab bar and the status line
	std::vector<item> items;
	int cols = 1;               // more than 1: a button grid
	int focus = 0, scroll = 0;  // focus: index into items; scroll: first visible row
};

struct tab { std::string name; std::vector<panel> panels; };
struct prompt { act a; std::string text; };

struct theme
{
	// the title's gradient and the outline of all lettering (U2.1); other text takes its
	// gradient from its one colour, see ink_for
	uint32_t title_top = 0xffffec60, title_bottom = 0xffff5a10, outline = 0xff000000;
	uint32_t bg_top = 0xff14102c, bg_bottom = 0xff301c5c, tab = 0xffb0b0d0, tab_sel = 0xffffffff,
	         panel = 0xff1c1638, panel_edge = 0xff5a4a9a, strip = 0xff3a2a78, strip_text = 0xffffe080,
	         text = 0xffc8d8ff, text_focus = 0xffffffff, text_off = 0xff707088, header = 0xff80ff80,
	         bar = 0xffffa020, value = 0xffffe080, status = 0xffb0b0b0, dialog = 0xff241a50;
	ink ink_for(uint32_t colour) const { return ink_of(colour, outline); }
	// art read from the user's ROMs for this theme (U2.3, themes.h); without it the screens use
	// the gradient and the project lettering
	std::vector<uint32_t> backdrop;                        // kW x kH pixels, painted instead of the gradient
	std::shared_ptr<const rom_art::font> title_font, list_font;
	int panel_alpha = 255;                                 // how solid the panels are over the backdrop
};

struct result
{
	enum what_t { none, activated, changed, back, context, yes, no } what = none;
	int id = 0;
};

class screen
{
public:
	std::string title;
	std::vector<tab> tabs;
	std::vector<prompt> prompts;        // extra prompts; move / select / back / tabs are added by themselves
	int cur_tab = 0, cur_panel = 0;

	result handle(const event &e);
	void draw(canvas &cv, const theme &th, device dev, unsigned now_ms) const;

	void ask(const std::string &question, int id) { m_dialog = question; m_dialog_id = id; m_dialog_yes = false; }
	bool asking() const { return !m_dialog.empty(); }
	void toast(const std::string &text, unsigned now_ms, unsigned for_ms = 2500) { m_toast = text; m_toast_until = now_ms + for_ms; }

	// focus
	item *focused();
	const item *focused() const { return const_cast<screen *>(this)->focused(); }
	void fix_focus();                   // after building or changing items: put focus on something usable
	bool focus_ok() const;              // on a usable item (or the tab has none)
	item *find(int id);

	// where things are on the canvas, for drawing, the mouse and the tests
	struct slot { int panel, index; rectf r; };
	std::vector<slot> slots() const;    // visible items of the current tab
	rectf panel_rect(int p) const;
	rectf tab_rect(int t) const;
	int rows_visible(int p) const;

	static constexpr int kW = 400, kH = 300, kRow = 12, kTabsY = 22, kTop = 38, kBottom = 268;

private:
	panel *cur() { return panels().empty() ? nullptr : &panels()[size_t(cur_panel)]; }
	std::vector<panel> &panels() { return tabs[size_t(cur_tab)].panels; }
	const std::vector<panel> &panels() const { return tabs[size_t(cur_tab)].panels; }
	bool move_in_panel(int step);       // by `step` items; false if nothing usable in that direction
	bool move_to_panel(int dx, int dy);
	void keep_visible(panel &p) const;
	bool scroll_text(int by);           // a panel with nothing to select: the directions scroll it
	result change(item &it, int dir);
	result press(item &it);
	void set_tab(int t);
	std::string m_dialog, m_toast;
	int m_dialog_id = 0;
	bool m_dialog_yes = false;
	unsigned m_toast_until = 0;
};

} // namespace ui
