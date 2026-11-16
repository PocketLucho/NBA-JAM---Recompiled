// lettering.h - the project's own menu lettering (roadmap U2.1): chunky capitals with a
// dark outline, a bevel and a top-to-bottom gradient, drawn for this project (the pixel
// drawings are in lettering.cpp; nothing is taken from the games).
//
// Three sizes on the 400 x 300 menu canvas:
//   note    7 pixels high   status line, hints
//   list    8 pixels high   rows, tabs, panel titles
//   title  16 pixels high   screen titles. It is the list drawing doubled with its corners
//                           smoothed, so the two always agree.
// Capitals, digits and ASCII punctuation. Lower-case letters are shown as capitals. A
// character the lettering does not have is shown as '?'; has_glyph tells them apart.
// Glyphs have their own widths, so text is measured in pixels, not characters.
#pragma once
#include "../rom_art.h"
#include "canvas.h"
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace ui
{

enum class face { note, list, title };

// colours of a piece of text: the gradient runs from top to bottom of the capitals
struct ink { uint32_t top, bottom, outline; };
// a gradient around one colour: lighter above, darker below
ink ink_of(uint32_t colour, uint32_t outline = 0xff000000);

// characters with a drawing of their own in the lettering: '\x01' and '\x02' are the
// up and down marks of a scrolling list
bool has_glyph(char ch, face f);
int text_h(face f);                                   // height of the capitals
int text_w(const std::string &s, face f);             // width in pixels, without the outline
// x, y: top left of the letters themselves; the outline reaches one pixel further out
// (and the title's shadow one more down and right)
void draw_text(canvas &cv, int x, int y, const std::string &s, face f, const ink &k);

// At most `px` pixels wide; a longer text ends in "..", never cut mid-letter.
std::string fit_px(const std::string &s, int px, face f);
// A window on a text that does not fit, moving with time and pausing at both ends.
std::string marquee_px(const std::string &s, int px, face f, unsigned now_ms);
// Lines no wider than `px`, broken at spaces; at most max_lines, the last one shortened.
std::vector<std::string> wrap_px(const std::string &s, int px, face f, int max_lines);

// The same three with any way of measuring text (a game's own lettering, see below).
using measure_fn = std::function<int(const std::string &)>;
std::string fit_px(const std::string &s, int px, const measure_fn &width);
std::string marquee_px(const std::string &s, int px, const measure_fn &width, unsigned now_ms);
std::vector<std::string> wrap_px(const std::string &s, int px, const measure_fn &width, int max_lines);

// ---------------------------------------------------------------- a game's own lettering
// Text in a font read from the user's ROM (rom_art.h). A character that font lacks comes
// from the project lettering instead, drawn at the font's height and in its colours
// (roadmap U2.4), so a string is always shown whole. x, y: top left of the capitals.
ink ink_from(const rom_art::font &f);                 // the font's own colours, top to bottom, and its outline
int text_w(const std::string &s, const rom_art::font &f);
// `tint` multiplies the font's colours: white leaves them, a grey dims them (a disabled row)
void draw_text(canvas &cv, int x, int y, const std::string &s, const rom_art::font &f, uint32_t tint = 0xffffffff);

// The whole character set in the three sizes, for the tests and for review.
void draw_sheet(canvas &cv, const ink &title, const ink &text);
// Mistakes in the drawings themselves (rows of unequal length...): empty when all is well.
std::vector<std::string> lettering_errors();

} // namespace ui
