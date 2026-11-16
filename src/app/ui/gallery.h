// gallery.h - one screen that shows every widget of the toolkit (U1.1). It is what the
// tests walk and draw, and what `NBAJamRecomp --ui-gallery` opens, so the toolkit can be
// tried with a real controller, keyboard, mouse or touch screen before any real screen is
// rebuilt on it.
#pragma once
#include "ui.h"

namespace ui
{
enum gallery_id { g_play = 1, g_ask, g_toast, g_sound, g_music, g_shader, g_volume, g_delay, g_grid0 = 100, g_list0 = 200 };

screen make_gallery();
// What the gallery does with a result: opens the dialog, shows toasts. Returns true when
// the caller should leave the gallery (Back outside a dialog).
bool gallery_react(screen &s, const result &r, unsigned now_ms);
}
