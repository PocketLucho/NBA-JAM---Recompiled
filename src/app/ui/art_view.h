// art_view.h - the test screen of the ROM art reader (roadmap U2.2): what rom_art read from
// one game's ROMs, a page at a time: its lettering, its team logos, each background. With
// nothing read (ROM missing, or nothing that checks out) the one page shows the project's
// own lettering instead, which is what every menu will do. `NBAJamRecomp --rom-art` opens
// it; tools/headless/arttest draws it.
#pragma once
#include "../rom_art.h"
#include "ui.h"

namespace ui
{

// a ROM picture on the canvas, clipped; `shrink` 2 draws every second pixel
void draw_picture(canvas &cv, const rom_art::picture &p, int x, int y, int shrink = 1);
// text in a ROM font, up to max_x; characters the font lacks are left out. Returns the x after it.
int draw_rom_text(canvas &cv, const rom_art::font &f, int x, int y, const std::string &s, int max_x);

int art_pages(const rom_art::art_set &s);
void draw_art_page(canvas &cv, const theme &th, const std::string &game, const rom_art::art_set &s, int page, device dev);

} // namespace ui
