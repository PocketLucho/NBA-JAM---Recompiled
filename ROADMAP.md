# NBA JAM Recomp roadmap

This file is the working plan for the project. Part 1 (widescreen and display), Part 2
(interface overhaul), Part 3 (Lua scripts), Part 4 (online) and Part 5 (releases and
platforms) are specified below.

**Order of work:**

1. Phase 0 (in Part 3): merge the two open pull requests. Everything touches
   `src/app/main.cpp`, so this goes first. **0.1 is done:** both are on `main`.
2. R1 and R2 (Part 5): automated builds, release pages and patch notes. They depend on
   nothing else, and every later milestone ships through them.
3. W1 (Part 1): scaling and border fixes. Small; can be released on its own.
4. Part 2, phases U1 to U7: the interface overhaul.
5. W2 to W6 (Part 1): true widescreen and ultrawide.
6. Part 3 from Phase 1 on: Lua scripts.
7. Part 4, phases N1 to N9: online rooms, spectating, queues and chat.
8. Part 5, phases R3 to R8: installers, macOS, Android and the first full release.

Two notes on doing R1 and R2 this early:

* The CI job in 0.2 and the pull request builds in R1.1 are the same workflow; build
  them together.
* R2.3 (news in the app) draws on the home screen, which Part 2 rebuilds. Do the feed
  reader and the setting in step 2 and put the notice on the current menu; U3 carries it
  over to the new one.

The interface comes before the Lua work so that the mod, controller and pause menus are
built once, in the new style. Each task names the model that should do it:

| Model | Use it for |
|---|---|
| **Opus** | Engine and netplay C++ changes, API design, reading the game sources, reverse engineering the ROM-only games, anything where a wrong address or a desync is possible |
| **Sonnet** | Writing scripts on a pattern Opus has already proven, headless tests, menu work, porting a finished script to the next game |
| **Haiku** | Mechanical work: tables, Spanish/English strings, README and `mods/README.txt` updates, regenerating symbol files, release notes |
| **Advisor** | Review at the end of every phase (the "Gate" line) before the next phase starts |

A task is done only when its check passes. If a check cannot be run (for example the ROM
is not available in the session), say so in the pull request instead of marking it done.

## Rules for every task

1. **No game data in the repository.** Addresses, symbol names and short opcode patterns
   are fine. ROM bytes, art, sound and text copied from the games are not.
2. **Scripts never change files on disk.** They patch the loaded copy of the program ROM
   or write live memory. The byte-identical builds stay byte-identical.
3. **Addresses are found, not hard-coded, where possible.** Use the symbol file for the
   revision, or an opcode pattern as `quarter_length.lua` does. A script that cannot find
   what it needs logs that and changes nothing.
4. **English only, for now.** The Spanish interface, `README.es.md` and `LEEME.txt` were
   removed at the owner's request; every user-visible string is written in English. Tasks
   below that say "Spanish/English" or "both languages" mean English only until a second
   language is brought back, at which point strings go through one table again (U1.5).
5. **Each script declares the games it supports**, and is tested on every one it declares.
6. **Tournament mode wins.** When a game's own tournament mode is on, scripts that grant
   power-ups or secret characters stay off, as the original does.
7. One pull request per task or small group of tasks, as a draft, with the test output.

## Where things stand

* Pull request
  [richvam/NBA-JAMs-Recomp#2](https://github.com/richvam/NBA-JAMs-Recomp/pull/2) is
  merged: `main` has Lua 5.4.6, the `nba` API, the **Lua mods** menu, `mods.ini`,
  `tools/headless/modtest`, and two scripts: `quarter_length.lua` and `no_select_timer.lua`.
* Pull request [richvam/NBA-JAMs-Recomp#1](https://github.com/richvam/NBA-JAMs-Recomp/pull/1)
  (controller remapping) is merged on top of it. The main menu now lists Lua mods,
  language, controls and about.
* Limits of the engine from pull request 2 that Part 3 has to remove:
  * Options are value lists only. There is no plain on/off switch, no per-player choice,
    no grouping, and no way to change anything while a game is running.
  * Scripts cannot share code (no `require`), so every script would repeat the same
    address lookups and game-state checks.
  * No named symbols. Scripts find addresses by opcode pattern only.
  * The netplay fingerprint covers the program ROM only. Two players with different
    scripts that write RAM pass the lobby check and desync later.
  * No input injection, no on-screen drawing, no events (game start, quarter, game end).
* Display, checked in `src/app/gl_present.cpp` and `src/app/main.cpp` and against a 16:9
  screenshot of Maximum Hangtime:
  * Every game renders 400 x 254 pixels, meant for a 4:3 tube. The 16:9 and 16:10 modes
    (F7) only stretch that picture sideways. Nothing extra is shown, and everything is a
    third too wide at 16:9.
  * With integer scaling on (the default), the height is the largest whole multiple of
    254 that fits: 1016 of 1080 lines, 2032 of 2160. The width follows from it, so the
    picture has a black border on all four sides even in a "widescreen" mode.
  * There are four fixed choices (4:3, 16:9, 16:10, fill). Nothing matches an ultrawide
    monitor except "fill", which stretches further still.
  * The game draws into video memory 512 pixels wide, of which 400 are shown. A true 16:9
    view at this pixel shape needs about 533 columns and 21:9 about 700, so real
    widescreen cannot be had by showing more of the existing memory.

---

# Part 1: Widescreen and display

Goal: every game fills 16:9, 16:10, 21:9 and 32:9 screens up to 4K correctly. "Correctly"
means two separate things, and the roadmap treats them separately:

* **Presentation:** whatever picture the game produces is scaled without distortion and
  without needless borders, at any window size.
* **True widescreen:** during play the game shows *more court* to the left and right
  instead of a stretched picture, with round players and a round ball.

## Phase W1: presentation fixes

No game changes. All in `src/app/`.

| ID | Model | Task | Check |
|---|---|---|---|
| W1.1 | Opus | Replace the four aspect choices with: **Original 4:3**, **Widescreen** (true widescreen where the game supports it, otherwise 4:3 centred; see W3), **Stretch** (today's behaviour, named honestly). The target shape comes from the window, not a fixed list, so any monitor works. Old `aspect=` values in the `.ini` map to the nearest new one. | Unit test of `output_rect` over the resolution list below |
| W1.2 | Opus | Scaling modes: **Integer** (as now), **Fit** (fills the height exactly; whole-number prescale followed by a linear step so pixels stay crisp at 4.25x or 8.5x), **Smooth**. Default becomes Fit. Integer never picks a size that leaves more than a set share of the screen empty without saying so. | Screenshots at 1080p and 4K: no top or bottom border in Fit; pixel columns even |
| W1.3 | Sonnet | Shaders at high resolution: scanlines, CRT mask and curvature are computed from the output size, so they look the same at 1080p, 1440p and 4K, and still line up when the picture is wider than 4:3. | Screenshot set per shader at three sizes |
| W1.4 | Sonnet | Fullscreen: borderless at the desktop resolution, correct on high-DPI displays (drawable size, not window size), multi-monitor aware. Overlay text and menus scale with the output instead of staying tiny at 4K. | Run under Xvfb at each size; menu text height is a fixed share of the screen |
| W1.5 | Sonnet | Side-fill choice for anything shown 4:3 on a wider screen: black, dimmed blur of the picture, or a user-supplied bezel image. | Screenshot of each |
| W1.6 | Sonnet | `tools/headless/viewtest`: renders one frame through the presenter at a given window size and writes a PNG, so every later check is one command. Resolution list used by all Part 1 checks: 1024x768, 1280x720, 1280x800, 1920x1080, 1920x1200, 2560x1080, 2560x1440, 2560x1600, 3440x1440, 3840x1600, 3840x2160, 5120x1440. | Runs in CI on a synthetic test pattern (no ROM) |
| W1.7 | Sonnet | **Overlays and borders** (added at the owner's request): PNGs in `overlays/` in the data folder are shown around the 4:3 picture as RetroArch does, over the game where they have a transparent opening, scaled to the screen height and centred. RetroArch border overlays (`.cfg` + `.png`) are read for their image. F8 goes through black, blur and each overlay. | `viewtest --fills` and `--rects` |

**Status:** W1.1, W1.2 and W1.6 are done. `src/app/view.cpp` places the picture from the window shape (Original 4:3, Widescreen, Stretch; Integer, Fit, Smooth; Fit is the default), and `tools/headless/viewtest` checks it: `--rects` over the resolution list without a display, and real frames of a test pattern through the presenter under Xvfb, both in CI with the PNGs as an artifact. The frames were drawn off-screen with software OpenGL; no real 1080p or 4K monitor was used. After the owner pointed out that "Widescreen" showed a 4:3 picture, F7 no longer offers it: the shape exists in `view.cpp` and is tested, but the app only cycles Original 4:3 and Stretch until a game really draws a wider picture (W3 switches it on per game). W1.3 is done: the CRT mask stripe follows the output height and starts at the picture's left edge; `viewtest --shaders` measures the mask period at 1080p, 1440p, 4K and 3440x1440 (3, 3, 6 and 3 pixels). W1.5 is done: F8 chooses black, the blurred and dimmed picture, or the user's `bezel.png` (read by a small PNG reader), checked by `viewtest --fills`. W1.4 is partly done: Windows per-monitor DPI awareness, a window sized from the screen, and fullscreen at the desktop resolution were added or confirmed, and menu and overlay text scale with the picture by construction (they are drawn into the 400-line picture). W1.7 is done: `src/app/overlay.cpp` lists the folder and reads RetroArch `.cfg` files, the presenter draws the image with its transparency, two original borders ship in `dist/NBAJamRecomp/overlays`; checked on rendered frames at 1920x1080, 2560x1080 and 5120x1440 and in a game on Windows. No overlay pack made for RetroArch by someone else was tried. The owner has asked for true widescreen (W2 to W5) to be looked into later, after the interface work, as the order of work already has it. Not checked for W1.4: a display at 150% or 200% scaling, and a second monitor; this machine has one 2560x1440 screen at 100%.

**Gate:** advisor compares screenshots at 1080p, 4K and 3440x1440 in all three modes. W1
can be released here.

## Phase W2: how the games draw (research)

Opus. No code ships from this phase; the output is a design note in the pull request.

| ID | Task |
|---|---|
| W2.1 | Trace the T.E. display path from the sources: the screen constants in `DISP.EQU` (`SCRNST`, `SCRNEND`, `SCRNMID`, the 56-pixel left pad), the blitter clip window, the object list's off-screen culling, the court scroll limits (`WRLDMID`, the player and scroll minimum and maximum in `PLYR.ASM`), and the background block drawing in `BAKGND.ASM`. List every place that assumes 400 columns. |
| W2.2 | Sort those places into **view only** (changing them shows more but the game plays the same) and **affects play** (for example anything the CPU players or the off-screen arrows decide from screen position). The aim is a widescreen that is view only, so a widescreen player and a 4:3 player can share a netplay game. If that proves impossible, say where and why. |
| W2.3 | Measure how much court art exists beyond the 4:3 view at each scroll position, per game. That sets the widest true view (it may be narrower than 32:9) and what happens at the ends of the court. |
| W2.4 | Classify every screen of each game: gameplay, or fixed screen (attract, team select, versus, halftime, stats such as "Best overall player", initials, test menu). Fixed screens are drawn for 400 columns and stay 4:3. |
| W2.5 | Repeat W2.1 to W2.4 for Hangtime and Maximum Hangtime from the sources, and note the Wolf Unit differences. NBA JAM 1993 by pattern match against T.E. and disassembly. |
| W2.6 | Choose the engine approach and justify it: a wider host-side video memory (the emulated pitch grows past 512 and the patched game addresses it), or keeping the game at 400 and capturing its draw list to re-render wider in the frontend. State the cost of each in game patches and in risk. |

**Gate:** advisor approves the design note before W3 starts. This is the decision the
rest of Part 1 rests on.

## Phase W3: true widescreen on T.E. 4.0

| ID | Model | Task | Check |
|---|---|---|---|
| W3.1 | Opus | Engine: variable visible width in `src/board/midway_video.cpp` and `tunit.cpp` per the W2.6 choice, off by default. With it off, output is bit-for-bit what it is today. | Frame hashes of a 4:3 headless run unchanged |
| W3.2 | Opus | Game patches for T.E. 4.0, applied in memory at load (never to files): clip window, culling, scroll limits, background fill. The view width is a parameter, not a fixed 16:9. | Headless tip-off at 533 columns: court drawn edge to edge, no garbage columns, players round |
| W3.3 | Opus | Screen switching: gameplay uses the wide view; fixed screens return to 400 columns centred with the W1.5 side fill. No flicker or one-frame stretch on the change. | Scripted run through attract, team select, game, halftime, stats |
| W3.4 | Sonnet | HUD: score, clock, turbo bars, player arrows and messages keep their place relative to the screen edges or centre as designed in W2. | Screenshot at 16:9 and 21:9 |
| W3.5 | Opus | Netplay: if W2.2 held, prove a 4:3 peer and a 16:9 peer stay in sync and keep widescreen out of the fingerprint. If not, add the view width to the fingerprint and the lobby. | `nettest`, 10 000 frames, equal hashes |

**Gate:** advisor plays a full game at 16:9 and 21:9.

## Phase W4: the other games

Sonnet ports, on the pattern W3 proved; Opus only where a game differs.

| ID | Model | Task |
|---|---|---|
| W4.1 | Sonnet | T.E. SE 5.02 (same code base; confirm patch sites by pattern). |
| W4.2 | Opus | Wolf Unit engine side (`wunit.cpp`), then Maximum Hangtime 1.3 from the sources. |
| W4.3 | Sonnet | Hangtime L1.3 / L1.1 and Maximum Hangtime 1.03 by pattern from W4.2. |
| W4.4 | Opus | NBA JAM 1993. |

Check for each: the W3.2 and W3.3 checks on that game.

## Phase W5: ultrawide

| ID | Model | Task | Check |
|---|---|---|---|
| W5.1 | Opus | 21:9 (about 700 columns) and 32:9 (about 1067) on every game. Where W2.3 found the art runs out, cap the view at the widest clean width and side-fill the rest. | Screenshots at 2560x1080, 3440x1440, 3840x1600, 5120x1440 at both ends of the court |
| W5.2 | Sonnet | Menu: a "Maximum view width" setting (16:9, 21:9, widest available) for players who find the widest view too zoomed out for play. | Setting round-trips through the `.ini` |
| W5.3 | Sonnet | Performance at 4K and 5120x1440 with the CRT shaders: full speed on integrated graphics, or the shader steps down and says so. | Frame time log |

## Phase W6: release

| ID | Model | Task |
|---|---|---|
| W6.1 | Haiku | READMEs and in-package docs: the new modes, keys, `.ini` settings, which games support true widescreen and how wide. |
| W6.2 | Sonnet | Full screenshot matrix: every game, every listed resolution, every mode, attached to the release pull request. |

**Gate:** advisor sign-off.

## Part 1 risks

* **True widescreen is a change to each game, not to the scaler.** The ROM-only games
  (NBA JAM 1993, Hangtime L1.3 / L1.1, Maximum Hangtime 1.03) are the hardest, as in Part 3.
* **The court may not be wide enough for 32:9.** W2.3 answers this; the fallback is a
  capped view with side fill.
* **Play could change.** If culling or scroll limits feed game logic, a wide view plays
  slightly differently and cannot mix with 4:3 online. W2.2 exists to find that early.
* **Part 3 depends on this.** The Lua overlay (`nba.text`, `nba.rect`) and the hitbox
  script use game-screen coordinates, which must follow the wide view.

---

# Part 2: Interface overhaul

Goal: every screen of the application looks like an NBA JAM menu and can be reached and
used with a controller alone. Layout and lettering follow the Sigil Jam T.E. editor
shown as the reference: tabs along the top, panelled lists with a solid highlight bar,
chunky gradient lettering, a status line along the bottom.

## Decisions

* **Font and art:** an original chunky gradient font, drawn for this project, ships with
  the application, so the menus always work. When the user's ROMs are present, the real
  lettering, team logos and backgrounds are read from them at run time and used instead.
  None of that art is ever stored in the repository or in a release. Real lettering is
  decoded only for the games with source (T.E., SE, Maximum Hangtime 1.3). NBA JAM 1993
  borrows the T.E. lettering when a T.E. ROM is present and Hangtime borrows Maximum
  Hangtime's; otherwise they use the project font. No font reverse engineering is done
  for the ROM-only games.
* **Theme per game:** one layout everywhere; colours, background and lettering follow the
  highlighted game (NBA JAM 1993, T.E. and SE, Hangtime, Maximum Hangtime). A neutral
  theme is used when a game's ROM is missing.
* **Inputs:** controller, keyboard, mouse and touch all work on every screen. A
  controller is enough for everything, including text entry.
* **Included:** menu music and sounds, a pause menu during play, an on-screen keyboard.

## What exists today

* Screens: game picker, online menu, lobby, about; plus Lua mods (pull request 2) and
  Controls (pull request 1). All are drawn as plain 8x8 text in `src/app/main.cpp`.
* A controller can move, confirm and go back in the picker, and Y opens the online menu.
  Everything else needs a keyboard: video mode, scaling, aspect, scanlines, fullscreen,
  screenshot, help, test switch, reset, pause, volume, service credit (F1 to F12, P, 9,
  - and +), typing the host's IP address, and language choice.

## Phase U1: menu toolkit

Opus. New files under `src/app/ui/`; `main.cpp` shrinks instead of growing.

| ID | Task | Check |
|---|---|---|
| U1.1 | **Widgets:** tab bar, panel with title strip, scrolling list with group headers (as the team headers in the reference), highlight bar, toggle, value picker, slider, button grid, status line, confirm dialog, toast message. Layout is in fractions of the screen, so it holds from 1024x768 to 5120x1440 using the W1 scaling. | `viewtest` screenshots of a widget gallery at four sizes |
| U1.2 | **Focus navigation:** every widget is reachable by direction pad or stick, with one rule for moving between panels and tabs (shoulder buttons change tab). Hold-to-repeat with acceleration in long lists. Focus is never lost and never lands on a disabled item. | Automated walk: from any screen, every item is reached by directions alone and Back always returns to the picker |
| U1.3 | **Unified input layer:** one set of menu actions (move, confirm, back, tab left/right, page up/down, context action) fed by controller, keyboard, mouse (hover, click, wheel) and touch (tap, drag to scroll). The Controls profiles from pull request 1 are respected in game; menus always use a fixed, standard mapping so a bad remap cannot lock anyone out. | Each input type drives the gallery in a scripted test |
| U1.4 | **Button prompts:** the status line shows what each button does on the current screen, with glyphs that match the connected controller (Xbox, PlayStation, Nintendo, keyboard) and switch when the last-used device changes. | Screenshot per controller family |
| U1.5 | **Strings:** every label goes through the Spanish/English table; text that does not fit is shortened or scrolled, never clipped mid-letter. | Both languages rendered on every screen |

**Status:** U1.1 to U1.5 are written in `src/app/ui/`: widgets, the one navigation rule (at the top of `ui.h`), the unified input layer with hold-to-repeat, text prompts per device family, and text fitting. `tools/headless/uitest` (67 checks, in CI) includes the automated walk: every usable item of the gallery is reached by directions and tab buttons alone, focus never rests on a header or a disabled item, and Back is reported from every state. `viewtest --gallery` draws the gallery at four sizes. `NBAJamRecomp --ui-gallery` opens it in the program. Open: no real controller, mouse or touch screen has been used (scripted input only, keyboard by hand); prompts are words, not glyph pictures; no string table, since the program is English only for now (rule 4).

**Gate:** advisor reviews the gallery and the navigation rule before any screen is rebuilt.

## Phase U2: lettering and themes

| ID | Model | Task | Check |
|---|---|---|---|
| U2.1 | Opus | **Project font:** an original bitmap font in the chunky, bevelled, gradient style: capitals, digits, punctuation, accented Spanish letters, in three sizes (title, list, small). Drawn new; not traced from the games. Gradient and outline colours come from the theme. | Full character sheet screenshot |
| U2.2 | Opus | **ROM art reader:** find and decode the lettering, team logos, backgrounds and menu palettes in each game's graphics ROM, using the image tables (`IMGTBL.ASM`, the `.TBL` files and `tools/imgfile.py`). Source games only; the ROM-only games borrow a sibling's lettering or use the project font. Decoded at run time into memory only. Any image that fails to decode falls back to the project art without an error on screen. | Each game: logos and lettering decoded and shown in a test screen; removing the ROM shows the fallback |
| U2.3 | Sonnet | **Themes:** a small theme file per game family (colours, gradient, background, highlight, which ROM images to use). The picker changes theme as the highlighted game changes, with a short cross-fade. | Screenshot of the picker on each game |
| U2.4 | Sonnet | Glyphs missing from a game's own font (accents, symbols, lower case if absent) come from the project font, matched in size and colour. | Spanish strings render fully in every theme |

**Status:** U2.1 is done, in English (rule 4): `src/app/ui/lettering.{h,cpp}` holds the project's own lettering, drawn for it on a pixel grid: capitals, digits and all ASCII punctuation, with a dark outline, a bevel and a top-to-bottom gradient, in three sizes (7, 8 and 16 pixels on the 400 x 300 menu canvas). The toolkit draws all its text with it and measures text in pixels, since the letters have their own widths; the gradient of the title and the outline colour are in the theme. `uitest` (79 checks) covers it and writes `font_sheet.png`, the full character sheet; `viewtest --gallery` shows the gallery in it at four sizes. Limits, so nobody assumes more: the title size is the list drawing doubled with smoothed corners, not a third drawing; lower-case letters are shown as capitals; the accented Spanish letters are not drawn and show as `?` until a second language returns (U2.4 is where missing glyphs are handled). Seen only in the test PNGs, not yet on a monitor in the program. U2.2 is done: `src/app/rom_art.{h,cpp}` reads lettering, team logos and backgrounds, with their palettes, from the user's program and graphics ROMs into memory; nothing is written to disk and no game art is in the repository. The lettering (the games' 128-entry ASCII font tables) and the screen-sized background modules are found by their shape, with no addresses, so the same code reads all five games; pictures are decoded as the blitter does (1 to 8 bits a pixel, with and without row compression). The team logo table could not be told from tables of players' faces by shape, so its place is listed per program revision (seven revisions, by CRC-32) and checked before use; an unlisted revision gets no logos. On the owner's ROMs every game is read: NBA JAM, T.E. and SE give 4 fonts, 27 logos and 8, 5 and 5 backgrounds; Hangtime and Maximum Hangtime 12 fonts, 29 logos and 7 backgrounds, in under two seconds for all five, and the pages were looked at one by one. `NBAJamRecomp --rom-art` is the test screen (lettering, logos, each background per game; the project lettering when a game has no ROM), `tools/headless/arttest` checks the reader on a made-up ROM (26 checks, in CI) including wrong, cut and missing ROMs, and `arttest --roms` lists what it reads from real ones. Different from the plan, for the owner to accept or reverse: the ROM-only games are read like the others instead of borrowing a sibling's lettering, because the shape search needed no reverse engineering. Not done or not checked: nothing in the menus uses this art yet (U2.3 chooses per theme, and must choose, since some backgrounds are photographs of people); T.E. was read from the `ROMKIT` `.IRW` files of the source ZIP, not from a MAME `nbajamte.zip`; the test screen was drawn headless and not opened in the program; the logos' team order is the games' own and is not yet named. U2.3 is written but its check waits for U3.1: `src/app/ui/themes.{h,cpp}` holds a theme per game family (NBA JAM; T.E. and SE; Hangtime; Maximum Hangtime) and a neutral one, as small `key = value` files that name colours, which ROM background to use (by size, optionally a part of it repeated, and how much to darken it) and which ROM fonts (by height) for titles and rows. `make_theme` puts that together with what U2.2 read, and `mix_themes` is the cross-fade (250 ms). Without ROM art the neutral theme is used whole. The gallery (`--ui-gallery`) has a Themes tab that repaints it in each theme with the fade. On the owner's ROMs every game gets its background and its own title lettering; Hangtime and Maximum Hangtime also draw rows and tabs in their own 9-pixel font, while NBA JAM, T.E. and SE keep the project lettering for rows because their smallest font (13 pixels) is taller than a row. Limits: the five theme files are built into the program in the file format, and `themes/<id>.theme` in the data folder replaces one; no theme files ship as separate files. The check "screenshot of the picker on each game" cannot be made until U3.1 rebuilds the picker; that is also where the theme starts to follow the highlighted game. The colours are first choices, for the owner to judge at the gate. U2.4 is done within rule 4: a character a game's font lacks is drawn from the project lettering at that font's height and in colours taken from it (`ink_from`), and `uitest` checks that every printable ASCII character shows something in a game's font. Lower case is shown as capitals, as the games' own tables do; the accented Spanish letters are not drawn (see U2.1), so the check "Spanish strings render fully" was not run. `uitest` has 98 checks and `arttest` 33. Not checked: any of it in the running program (headless PNGs only), and a real controller on the Themes tab. Added at the owner's request after U2, so the program shows the new look before U3 rebuilds each screen: the screens not yet rebuilt draw their text in the project lettering, in the same 8-pixel cells as before so no layout moved (`canvas::text`); words that need their lower case (file names, folders, web addresses) stay in the plain font. Checked by `uitest` and on a sample picture, not screen by screen in the program.

## Phase U3: rebuild the existing screens

Sonnet, on the U1 toolkit. Each screen replaces its plain-text version; behaviour stays.

| ID | Screen | Notes |
|---|---|---|
| U3.1 | **Game picker** | Games as a list with logo, year, board and status; missing games greyed with the reason in the side panel; the first-run case (no ROMs) explains where to put files. |
| U3.2 | **Settings** (new) | Tabs: Video (mode, scaling, shader, scanlines, side fill, fullscreen), Sound (volume, menu music, menu sounds), Game (free play, players, country, video clips), Language. Replaces the F4 to F7, F11 and volume keys, which keep working as shortcuts. |
| U3.3 | **Controls** | The profile editor from pull request 1 in the new style; binding a button is done by pressing it, with a controller-only way to cancel. |
| U3.4 | **Lua mods** | The mod list and mod pages from pull request 2 in the new style; this is the screen Part 3 extends. |
| U3.5 | **Online** | Host, join, lobby with player list and delay setting. |
| U3.6 | **About and help** | Controls reference generated from the current bindings, not fixed text. |

Check for each: `viewtest` screenshots in both languages, and the U1.2 automated walk.

**Status:** U3.1 and U3.2 are done, in English (rule 4). Both screens are built in `src/app/ui/screens.{h,cpp}` from a plain description that `main.cpp` fills, so the headless tests walk and draw the very screens the program shows. **Game picker (U3.1):** the games in a list, greyed when they cannot be played but still highlightable; beside it the highlighted game's name in that game's own large lettering, its revision, its board and either "ready to play" or the reason it is not; under the list the rest of the program (Online game, Settings, Lua mods, Controls, News, About, Look for ROMs again, Quit). With no game playable the side panel says where the ROM sets go. The theme follows the highlighted game with the U2.3 cross-fade, which also completes that task's check (the picker drawn on each game, from the owner's ROMs). Back asks before quitting. N and the context button still open the online menu for the highlighted game. **Settings (U3.2):** tabs Video (shape, scaling, look, scanlines, sides, fullscreen), Sound (volume), Game (free play, four players, country, video clips) and Program (check for new versions, which moved here from the menu). F4 to F8, F11, - and + keep working and the widgets follow them. Different from the plan: no picture logo per game (no game emblem is read from the ROMs; the name in the game's lettering stands in), year and board are in the side panel rather than in the list row, the Sound tab has no menu music or menu sounds because the program has none until U6, and there is no Language tab while there is one language. Checks: `uitest` (116 checks) runs the U1.2 walk on both screens and writes their PNGs; `arttest --roms` writes the picker on each game in its theme. The Linux build was also run under Xvfb and driven with scripted key presses: picker, theme change, Settings changed by widget and by F4, back, the quit question, starting T.E. and returning with Esc, the old Lua mods screen, and N for online. Not checked: the Windows build in use, a real controller, mouse or touch on these screens, and the first-run case in the running program (headless picture only). U3.3 and U3.4 are done the same way. **Controls (U3.3):** a Who panel (game, player, profile), the buttons of that profile with their bindings (a held one is marked), and a panel for the profile itself (what the two shortcuts press, dead zone, new, reset, delete, export, import). Confirm on an action waits for a press on the controller; the context button clears it; waiting five seconds or Esc cancels, so a controller alone is enough. A new profile's name can be typed, or taken as suggested with Enter or A. **Lua mods (U3.4):** the scripts in a list with on, off or error, and beside it the highlighted script's page: its whole description wrapped, the switch, all games or selected ones, its options, a switch per game. The context button switches a script on or off from the list; Confirm goes to its page and Back returns to the list. Checks: the U1.2 walk on both in `uitest` (129 checks) with their PNGs, and the Linux build under Xvfb with scripted keys: a script switched from the list and "apply to" changed on its page (both written to `mods.ini`), the game changed on Controls, an action waiting for a press and cancelled, a profile created. Not checked: binding with a real controller (no controller in the test run), export and import from the new screen, the Windows build in use. U3.5 and U3.6 are done the same way, and with them News, so no screen of the old layout is left. **Online (U3.5):** Host a game, Join a game, the host's address, the UDP port and the input delay, with this computer's local address under them and a short explanation beside them. The address and the port are typed (Enter or A takes them, Esc or B cancels; Ctrl+V pastes). The lobby lists the players with their address and ping; the host can still change the delay there, starts the game or closes it; a player who joined sees which player they are, the ping and what is being waited for, and can leave. **About and help (U3.6):** four tabs. About (version, build, latest release, where the files are, what the program is), Controls (for the chosen player, every action with its key from the settings file and its binding from the controller profile that player has for the highlighted game, so it is made from the current bindings and not fixed text), Keys (the program's own keys) and Credits. **News:** the notes of the latest release with their headings, scrolled with the directions. For these the toolkit gained one thing: a panel with nothing to select is scrolled by the directions, the page buttons and the wheel. Also fixed on the way: in a game's own lettering an apostrophe was drawn at the foot of the line like a comma. Different from before: the port is typed, not stepped by one with Left and Right; with only a controller the address and the port cannot be typed until the on-screen keyboard of U4. Checks: the U1.2 walk on Online, both lobbies, About and News in `uitest` (146 checks) with their PNGs; the Linux build under Xvfb with scripted keys: an address and a port typed, joining an address where nobody hosts (waits for an answer, then left), hosting, the delay changed in the lobby, a game started alone and left, News with no connection, the four About tabs with player 2 chosen. Not checked: two programs connected to each other through the new lobby (the netplay code is unchanged and `nettest` does not use the screens), News with real release notes in the running program (headless picture only), a real controller, the Windows build in use.

## Phase U4: on-screen keyboard

| ID | Model | Task | Check |
|---|---|---|---|
| U4.1 | Sonnet | A letter grid in the style of the games' initials entry, with layouts for text, numbers and IP addresses (digits and dot only). Opens wherever text is needed: host address, profile names, save-slot names. A physical keyboard still types directly, and paste still works. | Enter an IP address and a profile name with a controller only |
| U4.2 | Sonnet | Recent hosts list on the join screen, so an address is typed once. | Rejoin from the list in a scripted test |

## Phase U5: pause menu

| ID | Model | Task | Check |
|---|---|---|---|
| U5.1 | Opus | A controller shortcut opens the menu over the running game. Default: hold Start and Back together for half a second (and the Guide button, and Esc on the keyboard); the shortcut is configurable and can never be unbound entirely. The game is paused while it is open, offline. | Open and close in a headless run; game state hash unchanged across the pause |
| U5.2 | Sonnet | Contents: Resume, Video, Sound, Controls, Lua mods, Screenshot, Insert coin, Service credit, Test switch, Reset game, Quit to picker. This covers every function the F-keys and other keyboard-only keys have today. | Checklist: each old key has a menu entry |
| U5.3 | Opus | Online: the menu opens without pausing the game for anyone; items that would change the game (reset, test switch, mods) are disabled; Leave game asks for confirmation. | `nettest`: one peer opens the menu for 600 frames, hashes stay equal |

## Phase U6: menu music and sounds

| ID | Model | Task | Check |
|---|---|---|---|
| U6.1 | Opus | Play menu music and cursor, confirm and back sounds from the user's ROMs by running the game's own sound board with the right command numbers (T-Unit ADPCM board, Wolf Unit DCS), per theme. Nothing is extracted to disk. | Each game: a tune and three sounds play in a headless audio capture |
| U6.2 | Sonnet | Fallback when no ROM is present: short original sounds made for the project, or silence. Switches for music and sounds, separate volume, and the "Music on" indicator in the status line. | Settings round-trip; silence with both off |

## Phase U7: finish

| ID | Model | Task |
|---|---|---|
| U7.1 | Opus | **Controller-only audit:** unplug the keyboard in a scripted run and complete every flow: first start with no ROMs, pick a game, change every setting, rebind a controller, enable a mod, host and join online, pause and quit. Anything that needs a key is a bug. |
| U7.2 | Sonnet | Steam Deck and small screens: 1280x800 touch and controller check, minimum text size, safe margins for TVs. |
| U7.3 | Haiku | READMEs and in-package docs rewritten around the menus instead of the F-keys; strings reviewed in both languages. |

**Gate:** advisor sign-off on the audit and on screenshots of every screen in every theme.

## Part 2 risks

* **The real lettering and logos depend on decoding each game's art.** Only the source
  games are decoded, from their image tables. The others borrow or use the project font.
* **Team logos and player likenesses are NBA property.** They are only ever shown from the
  user's own ROM, never redrawn or shipped.
* **Not a copy of Sigil.** The reference sets the layout and feel; the font, colours and
  code are this project's own.
* **Later parts add screens.** The Lua option types (Part 3, task 1.3), the mods page in
  the pause menu (1.4), the draft screen and the hitbox switches must use the U1 widgets,
  so they inherit controller support.

---

# Part 3: Lua scripts

## Phase 0: land the foundation

| ID | Model | Task | Check |
|---|---|---|---|
| 0.1 | Sonnet | Rebase pull request 2 on `main`, resolve conflicts, get it ready to merge. Then rebase pull request 1 on top. | Linux app and `modtest` build; both existing scripts still pass their headless runs on T.E. 4.0 and SE 5.02 |
| 0.2 | Sonnet | Add a CI job that builds `libcore.a`, the Linux app and `modtest`, and runs the Lua engine tests that need no ROM (sandbox, budget, option parsing). | Job green on a pull request |
| 0.3 | Opus | Interim netplay guard: while netplay is active, refuse to start if any enabled script has an `on_frame` hook or writes RAM, with a clear message. Removed again in 1.7. | `nettest` with a RAM-writing mod enabled refuses to start |

**Status:** 0.1 is done. The merged tree builds the Linux app and `modtest`, and both
scripts pass their headless runs on NBA JAM rev 3.01, Hangtime L1.1 and Maximum Hangtime
1.03 (`quarter_length` at 1:30 starts every quarter at 1:30). T.E. 4.0 and SE 5.02 were
not rerun, because their program ROMs were not in the session. They have since been run
on a Windows machine (modtest built with clang, program ROMs from the user's source zips):
`quarter_length` at 1:30 starts the clock at 1:30 on both, and `no_select_timer` keeps the
game on team select where the unmodded game starts by itself. 0.2 is built together with
R1.1: `.github/workflows/release.yml`, the scripts in `tools/ci/` and the ROM-free
`tools/headless/luatest` are written and run by hand under WSL (Linux and Windows builds,
33 Lua engine checks, both packages). **0.2 and R1.1 are done:** the workflow is green on
pull request 5 (merged), and the test tag `v0.0.0-test1` produced a draft release with the
Windows zip, the Linux archive and `SHA256SUMS.txt`.

0.3 is done: `mod_manager::netplay_blockers` lists the enabled scripts that define
`on_frame` or mention `nba.write8/16/32`; host and client refuse to start an online game
while it is not empty, naming the scripts, and as a backstop RAM writes raise a script
error online. `nettest` (now with `MODS=<dir>`) refuses to start with a RAM-writing or
`on_frame` script, and still runs in sync with a ROM-only script on both sides. Note that
the shipped `quarter_length.lua` has an `on_frame` hook (for quarters over 9 minutes), so
it cannot be used online until 1.7.

**Gate:** pull request 2 merged; CI green.

## Phase 1: engine extensions

All Opus unless marked. Each one extends `src/app/mods.{h,cpp}` and `mods/README.txt`.

| ID | Task | Check |
|---|---|---|
| 1.1 | **Shared libraries.** `nba.include("lib/name.lua")`, limited to `mods/lib/`, no path escapes. Files in `lib/` are not listed as mods. | A mod using a lib runs; `../` is rejected |
| 1.2 | **Symbols.** `nba.sym("name")` returns the bit address for the running revision, or nil. Source-built games: `tools/gspasm` already writes a `.map`; add `tools/map2sym.py` to turn it into `mods/sym/<game>_<program CRC32>.lua` (names and addresses only). ROM-only games: the same file format, filled by hand in Phase 2, each entry carrying the opcode pattern that proves it. | `nba.sym("scores")` equals the `.map` value on T.E. 4.0, SE 5.02 and Maximum Hangtime 1.3 |
| 1.3 | **Option types.** Using the menu widgets from U1, add `type = "toggle"`, `type = "players"` (a tick per player 1 to 4, stored as a bit mask), `type = "range"` (min, max, step), and `group = "..."` headings. Options can declare `conflicts = { ids }`; the menu greys out the loser. Existing value-list options keep working. | Menu screenshot under Xvfb; `mods.ini` round-trips every type |
| 1.4 | **In-game mods menu.** The mods page is reachable from the pause menu built in U5, offline only. Options marked `live = true` apply at once through an `on_option(id, value)` hook; the others say "applies next game". | Toggle a live option mid-game in `modtest` and see the write |
| 1.5 | **Events.** `mods/lib/jam.lua` watches the game's own state variables and calls `on_matchup()`, `on_game_start()`, `on_quarter(n)`, `on_game_end()`. One implementation per game family, selected by `nba.game`. | Headless run to tip-off logs each event once, in order |
| 1.6 | **Input and drawing.** `nba.press(player, buttons, frames)` queues cabinet inputs (merged with the real ones before `set_inputs`, and part of the netplay input stream). `nba.text(x, y, str)`, `nba.rect(x, y, w, h, color)` and `nba.line(...)` draw on the frontend overlay in game-screen coordinates, never in the game's frame buffer. | Scripted start press reaches team select; overlay text and a box in a screenshot |
| 1.7 | **Netplay sync.** The host sends its enabled scripts, their options and a hash of each script and lib file with the game start. Clients apply exactly that set, or are told which file differs. The lobby lists the active scripts. Remove the 0.3 guard. Scripts get `nba.netplay` so a script can refuse to run online. | `nettest`: two peers with different local settings run 10 000 frames with a RAM-writing script and equal state hashes |
| 1.8 (Sonnet) | **Test harness.** Extend `modtest` with a `--expect` file: after N frames, assert that symbol X holds value Y. Add `tools/build_program_roms.sh`, which builds only the program ROMs of the three source games with `gspasm` (no art, no emu2), so any session can test against them. | One command builds the three program ROMs and runs every script's expectations |

**Gate:** advisor reviews the API as documented in `mods/README.txt` before any Phase 3
script is written. API changes after this point cost every script.

## Phase 2: code inventory

The scripts in Phases 3 to 5 are table-driven, so this phase produces the tables. Output:
one `mods/lib/codes_<game>.lua` per game, with one entry per code:

```lua
{ id = "tournament", label = { es = "...", en = "Tournament mode" },
  original = "111",            -- how the cabinet takes it, shown in the menu
  sym = "pup_tournament", kind = "flag",       -- or "players" for a bit per player
  when = "matchup", conflicts = { ... } }
```

| ID | Model | Task |
|---|---|---|
| 2.1 | Opus | **T.E. 4.0 and SE 5.02** from the sources (`SCORE.ASM`, `PLYR.ASM`, `SELECT.ASM`, `ATTRACT.ASM`). Record for each code the variable it sets, when the game clears it, and what tournament mode overrides. Diff SE against T.E. for added or changed codes. |
| 2.2 | Opus | **Maximum Hangtime 1.3** from the sources (`SELECT3.ASM`, `BB3.ASM`, `PLYR.ASM`, `COMBOS.DOC`). Note the rule that in human against human both sides must enter a number code. |
| 2.3 | Opus | **Hangtime L1.3 / L1.1 and Maximum Hangtime 1.03** (no source). Carry the 2.2 symbols over by opcode pattern; verify each in the disassembly (`tools/dasm`). |
| 2.4 | Opus | **NBA JAM 1993 rev 3.01** (no source). Start from the T.E. patterns for shared code, then disassemble. Slowest item in Part 3; needs the user's `nbajam.zip` in the session. |
| 2.5 | Haiku | Spanish and English labels for every entry; a table of all codes per game in the README. |

Already confirmed from the sources while preparing this roadmap:

**Hangtime and Maximum Hangtime, number codes on the versus screen**

| Code | Effect | Variable |
|---|---|---|
| 025 | Baby-sized players | `pup_baby` |
| 048 | No music during play | `pup_nomusic` |
| 111 | Tournament mode | `pup_tournament` |
| 120 | Fast turbo passes | `pup_fastpass` (bit per player) |
| 273 | Stealth turbo | `pup_trbstealth` (bit per player) |
| 284 | Maximum speed | `pup_maxspeed` (bit per player) |
| 390 | No pushing | `pup_nopush` (bit per player) |
| 461 | Infinite turbo | `pup_trbinfinite` (bit per player) |
| 552 | Hyper speed | `pup_hypspeed` (bit per player) |
| 616 | Shot blocking | `pup_maxblock` (bit per player) |
| 709 | Maximum steals | `pup_maxsteal` (bit per player) |
| 802 | Maximum power | `pup_maxpower` |
| 937 | Goaltending allowed | `pup_goaltend` (bit per player) |

**Hangtime and Maximum Hangtime, stick and button codes on the versus screen:** big head
(`pup_bighead`), huge head (`pup_hugehead`), show hot spots (`pup_showhotspots`), show shot
percentage (`pup_showshotper`), no tag arrows (`pup_notag`), no drift (`pup_nodrift`), no
computer assistance (`pup_noassistance`), ABA ball (`pup_aba`), outdoor court (`pup_court`),
the "special combo" and the vortex on the outdoor court (both to be traced in 2.2).

**T.E. 4.0, held or tapped on the matchup screen:** tournament mode (`plyr_tourn`, which
also resets `special_heads`), maximum power (`plyr_maxpower`), quick hands (`stlpower`),
big heads (`plyrbighead`), baby mode (`baby_flag`), shot percentage (`show_shot`), computer
assistance off (`assistance_off`), plus the tap-count power-ups read from `p1taps` to
`p4taps` and the `secret_1` to `secret_3` processes, which 2.1 must trace. Expanded
rosters and team switching are tap power-ups on team select (`SELECT.ASM`).

## Phase 3: code toggles

One script per game family, `codes_te.lua`, `codes_hangtime.lua`, `codes_jam.lua`, each
driven by its Phase 2 table. Every code is a switch in the menu; codes that the game
stores per player use the `players` option type so each player can be ticked.

How a code is forced: **write the variable at the moment the game would have set it**
(the `on_matchup` end for versus-screen codes), and write it again each frame only for
variables the game itself clears mid-game. Typing the code through injected inputs was
considered and rejected: it works on one screen only, cannot express "player 3 only",
and breaks the both-sides rule in Hangtime.

| ID | Model | Task | Check |
|---|---|---|---|
| 3.1 | Opus | `mods/lib/codes.lua`: the shared driver that turns a table into menu options and writes. Prove it on T.E. 4.0 with tournament mode, big heads and maximum power. | Headless: each variable holds the forced value at tip-off; with every switch off, RAM at tip-off equals an unmodded run |
| 3.2 | Sonnet | T.E. 4.0 and SE 5.02: all remaining codes. | Same check for every entry |
| 3.3 | Sonnet | Maximum Hangtime 1.3: all number and stick codes. | Same |
| 3.4 | Sonnet | Hangtime L1.3 / L1.1, Maximum Hangtime 1.03. | Same, on the user's ROMs |
| 3.5 | Sonnet | NBA JAM 1993. | Same, on the user's ROMs |
| 3.6 | Haiku | "Codes active" line on the overlay at tip-off; README section. | Screenshot |

**Gate:** advisor plays through the check output for one game per family and confirms
tournament mode suppresses what it should.

## Phase 4: unlock all hidden players

`unlock_players.lua`, with one implementation per family.

| ID | Model | Task | Check |
|---|---|---|---|
| 4.1 | Opus | Trace how each game admits secret characters: T.E. and SE through `special_heads` and the initials and birthday table; Hangtime through its secret-name and PIN table and the create-a-player `privileges` bits; NBA JAM 1993 by disassembly. Pick the mechanism per game. | Written note in the pull request |
| 4.2 | Sonnet | **Picker mode:** a menu option per player slot, "Normal" or any hidden character by name, applied at team select. No initials needed. | Chosen head and stats are in memory at tip-off |
| 4.3 | Sonnet | **Expanded rosters on** for T.E. and SE (the roster power-up), and **all create-a-player privileges on** for Hangtime and Maximum Hangtime. | Extra roster entries selectable in a scripted run |
| 4.4 | Opus | Make sure nothing here writes NVRAM records: a forced hidden player must not create or alter a saved player record. | NVRAM hash before and after a scripted game is unchanged apart from audits |

## Phase 5: handicap

`handicap.lua`: a head-start score for each team, 0 to 50, applied once at tip-off.

| ID | Model | Task | Check |
|---|---|---|---|
| 5.1 | Opus | Write `scores` at `on_game_start` and find what else the game derives from it (quarter score table `kp_qscrs`, the scoreboard refresh, the catch-up logic that reads the point gap, the end-of-game stats). Decide whether head-start points appear in the first-quarter box score. | Scoreboard shows the head start at tip-off; final stats screen is consistent |
| 5.2 | Sonnet | Menu: "Team 1 head start", "Team 2 head start", and "Only against the CPU" so it can stay on without affecting two-player games. Port to all games. | Headless on every supported game |
| 5.3 | Sonnet | Records protection: a game played with a handicap, or with any power-up script, does not count toward player records and win streaks. Offer it as a switch, default on. | NVRAM record unchanged after a scripted win |

## Phase 6: more scripts

Ordered by value for effort. Each is a separate small pull request, Sonnet unless marked.

**Rules and game flow**

| Script | What it does |
|---|---|
| `operator_settings.lua` (Opus first) | Every operator adjustment from the service menu (difficulty, game speed, head size, tournament default, hot spots, and so on) as menu options, without entering the test menu or changing NVRAM |
| `cpu_difficulty.lua` (Opus) | Drone skill level, and a switch for the rubber-band catch-up ("computer assistance") separate from the game's own code |
| `fire_rules.lua` | Baskets needed to catch fire (the game's `ball_numforfire`), always on fire, never on fire, per team |
| `turbo_rules.lua` | Infinite turbo per player on the games that have no code for it; turbo drain and refill rate |
| `rules.lua` | Shot clock off or its length, goaltending off, injuries off (T.E.), substitutions always offered |
| `full_game.lua` | A started game is paid through all four quarters, so no buy-in screens between quarters |
| `target_score.lua` | First to N points wins, and an optional mercy rule |
| `fast_boot.lua` | Skips the power-on ROM and RAM tests and goes straight to attract mode |
| `skip_screens.lua` | Skips halftime clips, the trivia contest (Hangtime) and the initials timer |

**Practice and analysis**

| Script | What it does |
|---|---|
| `practice.lua` | Frozen game clock, no shot clock, optional idle opponents, instant ball reset on a button |
| `show_info.lua` | Always-on shot percentage, turbo values, on-fire counters and an input display on the overlay |
| `stats_export.lua` (Opus for the API) | Writes the box score of each finished game to `stats/` as CSV and JSON. Needs a new write-only, folder-limited `nba.save()` |
| `cpu_vs_cpu.lua` | Full four-quarter game between two CPU teams for watching or streaming |
| `hitboxes.lua` (Opus first) | Fighting-game style boxes on the overlay: each player's body box, the reach used for steals, shoves and blocks while the move is active, the ball, the rim and the goaltending zone. Colours per type, a switch for each, and frame-step while paused. Opus traces the collision code (`COLL2.ASM` in T.E., the player and ball collision routines in Hangtime) and the world-to-screen projection; Sonnet ports it to the other games |

**Convenience**

| Script | What it does |
|---|---|
| `auto_initials.lua` | Enters each player's saved initials and PIN at the record screen through `nba.press`, so records load without typing |
| `random_matchup.lua` | Random team and player pairings, with an option to exclude teams |
| `sound_options.lua` | In-game music off and announcer off on games that have no code for them |

**Down the road: Fantasy Draft**

`fantasy_draft.lua` is the largest script in Part 3 and comes after everything above,
because it depends on the picker from Phase 4, the events from 1.5 and netplay sync from 1.7.
The draft happens inside the game session, and it has three ways to play:

* **Versus, offline:** 2 to 4 people at one machine draft, then play each other.
* **Versus, online:** the same over netplay.
* **Ladder, offline:** one or two people on the same side draft a pair, then take it
  through the game's normal run against every CPU team, exactly as a stock team would:
  the same opponent order, defeated-teams tracking, difficulty ramp and ending.

| ID | Model | Task | Check |
|---|---|---|---|
| 6.F1 | Opus | Trace how a selected team becomes two players on court: the roster tables, attribute tables, head and body art pointers, name plates and announcer name calls. Decide what can be mixed freely (any two players on one side) and what cannot (for example uniforms and team name on the scoreboard). | Written note; a hand-made mixed pair plays a headless quarter with correct heads, names and stats |
| 6.F2 | Opus | Draft screen shown in game, in place of team select: 1 to 4 drafters, snake order, a pick timer, the full player pool with attributes, hidden characters included when `unlock_players.lua` allows them. Picks are made with the normal controls. | Scripted four-player draft ends with four distinct legal picks |
| 6.F3 | Sonnet | Options: draft order (random, loser picks first, fixed), pool (current rosters, expanded rosters, everyone), no duplicate players, and saved draft results that can be replayed. Port to every game. | Headless on each supported game |
| 6.F4 | Opus | Netplay: the draft runs in lockstep as part of the game's input stream, so all peers see the same picks. | `nettest` draft with four peers, equal hashes |
| 6.F5 | Opus | **Ladder mode.** Trace how the game tracks the run against the CPU: the teams-defeated record tied to the player's initials, the next-opponent choice, the difficulty ramp and the end sequence. Make a drafted pair count as one entrant in that run, kept for the whole ladder (the pair is stored with the script's settings, not in the game's NVRAM record format). Options: CPU opponents keep their real rosters, or the CPU drafts too from what is left. | Scripted run: drafted pair wins two games in a row, the second opponent is the one the stock game would pick, and the pair is unchanged |
| 6.F6 | Sonnet | Ladder save slots: several drafted pairs, each with its own progress, resumable from the mods menu. Port to every game. | Quit and resume mid-ladder in a headless run |

Each script gets the same treatment as Phase 3: symbols from the table, a headless
expectation, both languages, the README list updated by Haiku.

## Phase 7: release

| ID | Model | Task |
|---|---|---|
| 7.1 | Sonnet | Package `mods/`, `mods/lib/` and `mods/sym/` in the Windows and Linux builds and in `release.yml` |
| 7.2 | Haiku | `README.md`, `dist/NBAJamRecomp/README.txt`, `mods/README.txt`: script list, API reference, netplay behaviour |
| 7.3 | Opus | Final netplay soak: each script on, two and four peers, 30 000 frames, hashes equal |

**Gate:** advisor sign-off, then tag the release.

## Known risks

* **NBA JAM 1993 and the ROM-only Hangtime revisions** have no symbols. Every address
  there is reverse engineered, and those ROMs must be supplied by the user in the working
  session; they cannot be stored in the repository or downloaded by an agent. Uploads do
  not carry over between sessions, so each session that tests on them needs `nbajam.zip`,
  `nbahangt.zip` and `nbamht.zip` attached again. `nbajamte.zip` is only needed for the
  T.E. sound ROMs; the T.E., SE and Maximum Hangtime 1.3 programs build from the public
  sources.
* **Per-frame writes can fight the game.** Prefer one write at the right moment over a
  write every frame, and prefer a ROM patch in `on_load` when the value is a constant.
* **Desyncs.** Any script that reads real time, random numbers outside the game, or local
  settings will desync netplay. Scripts get no clock and no `math.random` seed of their own.
* **Hot `main.cpp`.** The app is one 1 000-line file that pull requests 1 and 2 both
  change. Keep new menu code in `mods.cpp` and its own files.

---

# Part 4: Online

Goal: rooms in the style of Ember for Street Fighter IV. Public and private rooms, tables
with seats, queues, spectating, chat in the room and during a game, all usable with a
controller, at no running cost to the project.

## Is it possible without servers?

Yes, with one honest qualification. Players cannot find each other on the internet
without *some* shared meeting point, so "no servers" becomes "no servers the project
pays for or maintains". Everything heavy stays between the players:

| Job | Where it runs | Cost |
|---|---|---|
| Game inputs, state checks | Player to player (UDP), as today | None |
| Room chat, game chat, seats, queues | Player to player, through the room's host | None |
| Spectator feed | From the table's host, or passed along by other spectators | None |
| **Room list and introductions** | A tiny service on a free hosting tier, with public relays as fallback | None, within daily limits |
| Finding your own public address | Free public STUN servers | None |
| Relay for the few connections that cannot go direct | Another member of the room | None |

What this buys and what it does not:

* Rooms keep working if the meeting point is down or over its limit: a private room code
  or a direct IP address still connects, and rooms already open are unaffected.
* Most home connections can connect directly once introduced. A minority (strict or
  carrier-grade NAT on both ends) cannot; they are relayed through a room member who
  can, or told plainly that they need a virtual LAN such as Tailscale.
* The free tier has daily request limits. The design spends requests only on listing
  and joining rooms, never on chat or play, which should cover hundreds of players a
  day. That figure is an estimate; N3.1 measures it against the limits in force then.
* Someone has to own the free account the room list runs in. That is the project owner;
  it needs no payment card and no upkeep beyond deploying it once.

## Decisions

* **Meeting point:** Cloudflare free tier first; public relay network as fallback.
* **Netcode:** lockstep now, improved. Save states are built as a foundation. Rollback is
  a later phase with its own feasibility check.
* **Identity:** a nickname only. No accounts, no sign-in.
* **Order:** after Part 3, so tables show and sync their Lua scripts from the start.

## What exists today

* Lockstep with input delay over UDP, star topology, up to four players, one game per
  host. The host must forward UDP port 7845 or everyone must use a virtual LAN. Joining
  means typing the host's IP address.
* Every game starts from power-on on all machines with the same NVRAM and settings.
* The emulator has no save states: `save_item` in `src/emu/emu.h` is an empty stub. So
  nobody can join a game in progress, reconnect after a drop, or recover from a desync.
* No spectators, no chat, no room list.

## Phase N1: save states

Opus. The foundation for instant spectating, reconnect, desync recovery, replays and
rollback. Useful offline too.

| ID | Task | Check |
|---|---|---|
| N1.1 | Implement `save_item` registration in `src/emu` and serialise every device: TMS34010, the sound CPUs (M6809, ADSP-2105), YM2151, OKI and DCS state, video RAM, blitter state, palette, NVRAM, protection and security PIC state, on both boards. | Save at frame N, run 3 000 frames, record hashes; load, run 3 000 frames; hashes equal on every game |
| N1.2 | Audio continues cleanly across a load (no click, no drift), and the Lua engine's per-script state is saved with the machine. | Audio capture across a load; a script counter survives |
| N1.3 | Size and speed: a state small and fast enough to send to a joining player and, later, to take every frame. Report bytes (raw and compressed) and microseconds per save and load. | Numbers in the pull request; they feed N9.1 |
| N1.4 (Sonnet) | Offline save slots in the pause menu, off by default, and never available online. | Save and load from the menu in a scripted run |

**Gate:** advisor reviews the determinism results. Nothing else in Part 4 that touches a
running game starts before this passes on all five games.

## Phase N2: connecting without port forwarding

Opus.

| ID | Task | Check |
|---|---|---|
| N2.1 | **Automatic port mapping** (UPnP, NAT-PMP, PCP) when hosting, removed on exit. | Works against a test router implementation in CI |
| N2.2 | **Hole punching:** learn the public address from STUN, exchange candidates through the meeting point (N3), punch both ways, keep the mapping alive. IPv6 used directly when both sides have it. | Simulated NAT matrix (full cone, restricted, port restricted, symmetric): expected pairs connect |
| N2.3 | **Peer relay:** when two members cannot connect directly, a third member both can reach forwards their packets. Chosen automatically, shown in the room, bandwidth capped. | Symmetric-to-symmetric pair plays through a relay in the simulator |
| N2.4 | **Connection report:** each player sees their own connection type (open, moderate, strict) and, per opponent, ping, jitter and whether the path is direct or relayed, as the wired and wireless icons in the reference. | Screenshot; values match the simulator's settings |
| N2.5 | **Mesh for four players:** inputs go directly between all players instead of through the host where that is faster, falling back to the host as hub. | `nettest` with four peers at uneven latencies: delay follows the worst direct pair, not the sum through the host |

## Phase N3: meeting point

| ID | Model | Task | Check |
|---|---|---|---|
| N3.1 | Opus | **Protocol and budget.** Define the few messages: publish room, refresh, remove, list by game and region, introduce two players. Count requests per player per session and set limits so the free tier holds; write down the limits found at the time. | Budget table in the pull request |
| N3.2 | Opus | **Cloudflare service** under `server/`: room list with expiry, introduction exchange, rate limits per address, size limits on every field, no chat and no game data. One-command deploy, documented for the project owner. The service address is a setting, so anyone can run their own. | Local emulator tests; deploy guide followed from scratch by Sonnet |
| N3.3 | Opus | **Public relay fallback:** the same messages over a public relay network, signed with a per-session key so entries cannot be forged in transit. Used when the first service is unreachable or over its limit; results merged and de-duplicated. | Room listed through the fallback only is found and joined |
| N3.4 | Sonnet | **Private rooms:** not listed anywhere. Joined by a short room code or a pasted link; the code alone is enough to connect when the host is directly reachable, so private rooms work with no meeting point at all. | Join by code with the service switched off |
| N3.5 | Sonnet | **Regions and latency:** rooms carry a region; the list shows "near you" from measured ping, not from guessing. | List sorted correctly in the simulator |

## Phase N4: rooms, tables and queues

Opus designs the room protocol; Sonnet builds the screens on the Part 2 toolkit.

| ID | Model | Task | Check |
|---|---|---|---|
| N4.1 | Opus | **Room model**, run by the room host and replicated to members: up to 16 members, several tables, each table one game with four seats (two per team). Room host migration when the host leaves, so the room survives. | Kill the host in a 6-member simulated room; room continues with the same tables |
| N4.2 | Opus | **Table rules:** which game and revision, quarter length, CPU fills empty seats or not, winner stays or rotate, number of games, input delay automatic or fixed. Every member's ROM fingerprint is checked against the table's game before they can sit. | Mismatched ROM is refused with the reason |
| N4.3 | Sonnet | **Queues:** join a table's queue as a single or as a pair; when a game ends, losers leave and the next in line sit, per the table rule. "Wait in practice" lets a queued player play offline until called, with a clear call-up and a countdown. | Scripted three-pair rotation over four games |
| N4.4 | Opus | **Scripts per table:** the table lists its active Lua scripts and options (Part 3, task 1.7) before anyone sits; sitting accepts them. Fantasy Draft tables run the draft for the seated players. | Member with different local scripts sits and stays in sync |
| N4.5 | Sonnet | **Screens:** Online home, Public rooms (quick join, create room, enter code, filters), Room (tables, members, chat, actions), Table options. Laid out as in the reference, in the per-game theme, with the button prompts along the bottom. | Screenshots in both languages; controller-only walk |
| N4.6 | Opus | **Quick join:** the open room near you with a free seat for a game you own, else the shortest queue. | Simulator picks the expected room |
| N4.7 | Opus | **Reconnect:** a dropped player's seat is held for a short time; the game pauses or the CPU takes over per table rule; on return they receive a save state and resume. | Drop and return in `nettest`; hashes equal afterwards |

## Phase N5: spectating

| ID | Model | Task | Check |
|---|---|---|---|
| N5.1 | Opus | A spectator receives a save state and then the merged input stream, a few seconds behind live, and runs the game locally. Join at any moment. Spectators never send anything the game depends on, so they cannot cause lag or a desync. | Join at frame 20 000; spectator hashes match the players' |
| N5.2 | Opus | **Fan-out:** the table host serves a few spectators; further spectators are fed by earlier ones, so a popular game costs the players nothing extra. | 12 simulated spectators; host sends to at most the set number |
| N5.3 | Sonnet | Spectator screen: score bug, who is in which seat, "watching" count, cycle between tables, "next game" seat request, optional delay increase for streamers. | Screenshots; controller-only |
| N5.4 | Sonnet | **Replays:** a finished game is saved as its starting state plus inputs; watch later, with pause and fast forward. Tied to the exact ROM and script set. | Replay of a full game ends with the recorded final hash |

## Phase N6: chat

| ID | Model | Task | Check |
|---|---|---|---|
| N6.1 | Opus | **Transport:** room chat goes through the room host, game chat between the table's players and spectators, all on a channel separate from game inputs so chat can never stall or desync a game. Length and rate limits enforced by every receiver. | Flood from one peer: game unaffected, messages dropped |
| N6.2 | Sonnet | **Room chat panel** as in the reference, with the on-screen keyboard from U4 and a physical keyboard when present. | Send and receive with a controller only |
| N6.3 | Sonnet | **In-game chat:** an overlay that does not cover the play area in widescreen, opened with a button; a wheel of quick phrases (in both languages, translated on the receiving side) for controller players, plus free text. Separate switch to hide spectator chat while playing. | Screenshots at 4:3 and 16:9; quick phrase arrives in the receiver's language |
| N6.4 | Sonnet | **Controls for the reader:** mute a player, hide all chat, word filter on by default, links never clickable. | Settings honoured in a scripted room |

## Phase N7: names and moderation

Nickname only, as decided. This phase makes that workable and states its limits.

| ID | Model | Task | Check |
|---|---|---|---|
| N7.1 | Sonnet | Profile: nickname, region, favourite team, local win and loss record. Names are cleaned (length, characters, look-alike letters) and duplicates in a room get a suffix. | Two "JAM" in a room show as distinct |
| N7.2 | Opus | Room host tools: kick, ban for the life of the room (by network address, the only handle available without identities), lock room, hand over host. Members: mute and block, remembered locally by name and address. | Kicked peer cannot rejoin the room in the simulator |
| N7.3 | Opus | Public list protection: limits on rooms per address, name filter on room titles, and a report action that hides a room locally. | Abuse cases in the service tests |

Limits to state in the docs: without accounts, a name can be copied by anyone and a
banned player can return from a new address. If that becomes a real problem, a
device key (silent, no sign-up) can be added later without changing the room protocol;
N4.1 leaves a field for it.

## Phase N8: release

| ID | Model | Task |
|---|---|---|
| N8.1 | Opus | Soak: 16 simulated members, three tables, spectators, chat, two host migrations, a drop and reconnect, for an hour of game time; all hashes equal. |
| N8.2 | Opus | Security pass over everything received from the network: every length checked, save states from peers validated before loading, no file paths or script code ever accepted from a peer (scripts are matched by hash, never transferred). |
| N8.3 | Sonnet | Protocol version in every message, so old and new builds refuse each other politely. Direct IP play kept as a menu entry. |
| N8.4 | Haiku | Online guide in both languages: rooms, codes, connection types and what to do about a strict one, how to run your own room list. |

**Gate:** advisor sign-off, then a public test weekend before the release is tagged.

## Phase N9: rollback (later, after a feasibility check)

| ID | Model | Task |
|---|---|---|
| N9.1 | Opus | **Feasibility:** from the N1.3 numbers, can the emulator save every frame and re-run up to 8 frames inside one frame's time on a modest machine, with sound handled separately? Go or no-go, with figures. |
| N9.2 | Opus | If go: rollback for two-player tables first, input prediction, sound and Lua overlay made rollback-safe; lockstep stays as the fallback and for four players until proven. |
| N9.3 | Sonnet | Per-table choice of netcode, shown in the room; mixed tables refused. |

## Part 4 risks

* **Free tiers and public relays can change or vanish.** That is why there are two, why
  the address is a setting, and why private codes and direct IP need neither.
* **Strict NAT on both sides** has no free guaranteed fix. Peer relay covers most cases.
* **Public chat with no accounts** invites abuse. The tools are local mute and block,
  host kick, and filters. There is no global ban and nobody to appeal to.
* **Save states are the critical path.** Spectator join, reconnect and rollback all wait
  on N1; a device whose state is missed shows up as a desync minutes later.
* **Four-player lockstep is only as good as the worst connection.** Rooms should be
  regional; the list makes distance visible before a seat is taken.
* **Not a copy of Ember.** The reference sets the feature list and feel; the protocol,
  layout details and code are this project's own.

---

# Part 5: Releases and platforms

Goal: anyone can install the game on Windows, Linux, macOS or Android in a couple of
clicks, see what changed in each version, and play online against any other platform.

## What exists today

* `.github/workflows/release.yml` is an empty file. Builds are made by hand.
* `releases/` holds a Linux archive committed to the repository. The README also links a
  Windows zip there, but `*.zip` is ignored by git, so that file is not in the repository.
* Windows is cross-compiled from Linux with llvm-mingw; Linux is x86-64 only. There is no
  macOS or Android build.
* Settings, saves, screenshots and `roms/` all live next to the executable. That breaks
  as soon as the program is installed somewhere it cannot write, and does not exist as a
  concept on Android.
* Versions are dates (2026.11.06). Nothing in the program knows its own version.

## Decisions made here (change any of them)

* **Free distribution only:** GitHub Releases for every platform. No store fees. The
  consequences for macOS and Windows warnings are under "Limits" below.
* **Windows:** an installer and a portable zip. **Linux:** an AppImage and a tar archive.
  **macOS:** one universal app (Apple Silicon and Intel) in a disk image. **Android:** an
  APK to sideload, 64-bit ARM, Android 8 or later.
* **Cross-play means bit-identical emulation** on every processor and compiler, because
  the netcode compares game state between players. This is tested, not assumed.

## Phase R1: automated builds

| ID | Model | Task | Check |
|---|---|---|---|
| R1.1 | Sonnet | Fill in `release.yml`: build Windows x64 and Linux x64 on every pull request, and on a version tag build, package and attach everything to a draft GitHub Release. Dependencies (SDL2, toolchains) are fetched and cached by the workflow, with pinned versions and checksums. | A test tag produces a draft release with both packages |
| R1.2 | Sonnet | The program knows its version, build date and commit; shown in About, in the status line and in logs. Online protocol version (N8.3) is separate from the program version. | Version string matches the tag |
| R1.3 | Sonnet | Remove built binaries from the repository (`releases/`), point the README at the Releases page, and add a check that fails a pull request containing ROM-like or archive files. | Repository holds no binaries; the check catches a test file |
| R1.4 | Opus | **User data folder.** Settings, saves, screenshots, mods, replays and `roms/` move to the per-user location each system expects, behind one path layer. A `portable.txt` next to the executable keeps today's behaviour. First run of a new version offers to move an existing setup. | Installed copy under a read-only folder runs and saves; portable copy unchanged |

**Status (R1):** R1.1 is done (see Part 3, Phase 0). R1.2 is done: the version comes from the nearest `v*` git tag (`src/app/version.cpp`, Makefile `APP_VERSION`), is shown in About, on the main menu and in the log, and `--version` prints it; CI fails a tagged build that does not report its tag (checked locally with a test tag; first real tag will confirm it on GitHub). The version is not yet in the window title or the in-game overlay; the status line of the new menus (U1.1) should carry it. R1.3 is done: `releases/` is gone from the repository and ignored, both READMEs point at the Releases page, and `tools/ci/check_repo.py` (a CI job on every pull request) refuses archives, ROM-like files and executables by extension, by content and by ROM-like size; it was shown to catch test files of each kind. The old archive is still in the git history; removing it there would mean rewriting history. R1.4 is done: `src/app/paths.{h,cpp}` decides the data folder (`portable.txt` next to the executable keeps everything there; otherwise the per-user folder; `NBAJAM_DATA_DIR` overrides), the archives ship with `portable.txt`, an installed copy seeds `mods/` and `roms/` and offers to move an older setup. Checked: 24 ROM-free tests (`tools/headless/pathtest`, in CI), and under Xvfb an installed copy in a read-only folder runs and writes its settings to the user folder while a portable copy writes nothing there. On Windows the move dialog was run with each answer (Move, Here, Later), a fresh copy filled the per-user folder under `%APPDATA%`, and a game saved its NVRAM there. Not checked: a move across two real drives, and what the user sees when the per-user folder cannot be created (the program falls back to its own folder silently).

## Phase R2: release pages and patch notes

| ID | Model | Task | Check |
|---|---|---|---|
| R2.1 | Sonnet | `CHANGELOG.md` kept by a rule: every pull request adds a line under "Unreleased" in one of Added, Changed, Fixed, Online, Known issues. CI fails without it unless the pull request is marked as needing none. | CI check |
| R2.2 | Haiku | At release time, turn "Unreleased" into the release page in English and Spanish: a short summary, the lists, which downloads are for which system, first-run steps, and whether this version can play online with the previous one. | Reviewed draft for the first automated release |
| R2.3 | Sonnet | **News in the app:** the home screen shows the latest release notes and a "new version available" notice, read from the public GitHub Releases feed. It only informs; it never downloads or installs by itself. Can be switched off. Works offline without errors. | Screenshot; no network: no notice and no delay |
| R2.4 | Haiku | README rewrite in both languages around what a player needs: download, where ROMs go, first run, controls, online, mods, troubleshooting. Build instructions move to `docs/BUILDING.md`. Kept current by R8.3. | Advisor read-through |

**Status (R2):** R2.1 is done (the check failed a test pull request with no entry and passed it once labelled): `CHANGELOG.md` with an Unreleased part (Added, Changed, Fixed, Online, Known issues), and the `changelog` workflow fails a pull request that adds no line there unless it has the `no-changelog` label (`tools/ci/check_changelog.py`). R2.2 is written: `tools/ci/release_notes.py` builds the release page from the changelog (lists, downloads per system, first-run steps, online compatibility from the protocol version) and the release job uses it; `cut` turns Unreleased into a version section before tagging. Open: the owner reviews the first draft, and the job has not yet run on a tag. R2.3 is done on the current menu (U3 carries it to the new one): `src/app/news.{h,cpp}` asks the Releases feed once, in the background, at start; the main menu shows "New version available", the News screen shows the notes, About shows the latest release; `news=0` or the menu switches it off. Checked on Windows with a stand-in feed answer, with the real feed (no release published yet: no notice), with an unreachable address (no notice, no delay) and switched off; 27 ROM-free tests in CI. On Linux the request (made by the system curl) was run from a test program. Not checked: a real published release of this project, and the screens on Linux. R2.4 is written: `README.md` is rewritten around what a player needs (download, ROMs, first run, controls, online, mods, troubleshooting) and the build, test and release instructions are in `docs/BUILDING.md`. English only, per rule 4. Open: the owner reads it through.

## Phase R3: Windows installer

| ID | Model | Task | Check |
|---|---|---|---|
| R3.1 | Sonnet | Installer built in CI with a free tool (Inno Setup): per-user install with no administrator prompt, Start menu and optional desktop shortcut, uninstaller that leaves the user's ROMs and saves unless asked. English and Spanish. | Silent install and uninstall on a CI Windows runner |
| R3.2 | Sonnet | First-run ROM step: a screen with "Open my ROM folder" and drag-and-drop of files or zips onto the window, then a rescan. | Scripted first run from empty to a playable game |
| R3.3 | Opus | Signing: apply to a free signing programme for open-source projects; if accepted, sign the executable and installer in CI. If not, document the Windows warning and publish checksums. | Decision recorded |

## Phase R4: Linux packages

| ID | Model | Task | Check |
|---|---|---|---|
| R4.1 | Sonnet | AppImage built on an old enough base to run on current distributions and on Steam Deck, with desktop entry and icon; the tar archive stays. | Runs in three distribution containers |
| R4.2 | Sonnet | Linux ARM64 build, as the first non-x86 target and the first test of R5. | Runs under emulation in CI |

## Phase R5: identical emulation everywhere

Opus. The requirement that makes cross-play possible. Do this before the ports.

| ID | Task | Check |
|---|---|---|
| R5.1 | **Audit** everything that feeds game state for behaviour that differs by processor or compiler: `char` signedness (unsigned on ARM Linux and Android), shifts of negative numbers, integer overflow, uninitialised memory, struct layout in save states, byte order assumptions, and any floating point (found in the board and sound code) that can reach the main CPU through the sound board's status or timing. | Written findings; each fixed or proven harmless |
| R5.2 | **Golden hashes:** for each game with a buildable program ROM, a recorded input script and the state hash every 600 frames. CI runs it on x86-64 and ARM64, with GCC, Clang and MSVC-compatible builds, and compares. | Identical on every target |
| R5.3 | Save states (N1) are byte-identical across platforms, so a phone can receive one from a PC. | Save on one target, load on another, hashes equal |
| R5.4 | The Lua engine gives the same results everywhere: number formatting, integer and float behaviour, table iteration order never used for game writes. | Script test suite identical on every target |

**Gate:** advisor reviews the audit. No port is called cross-play capable before R5.2 is
green on its processor.

## Phase R6: macOS

| ID | Model | Task | Check |
|---|---|---|---|
| R6.1 | Opus | Native build for Apple Silicon and Intel as one universal app, built on a macOS CI runner. Rendering through the existing OpenGL path first; move to Metal (through SDL's GPU layer or a translation layer) only if OpenGL proves unreliable. Retina sizes handled as in W1.4. | App launches and renders on both processor types in CI |
| R6.2 | Sonnet | Mac conventions: app bundle and icon, Command-key shortcuts, fullscreen and Spaces, data in Application Support, common controllers including PlayStation and Xbox pads over Bluetooth. | Checklist with screenshots |
| R6.3 | Sonnet | Disk image with drag-to-Applications, built in CI. Ad-hoc signed so it runs on Apple Silicon. First-launch instructions for the unidentified-developer warning in the notes and README. | Image mounts and the app runs on a clean runner |
| R6.4 | Opus | Online on macOS: local-network permission prompt handled, firewall prompt explained, R5.2 green, a Mac against Windows and Linux in `nettest`. | Cross-platform game, equal hashes |

## Phase R7: Android

| ID | Model | Task | Check |
|---|---|---|---|
| R7.1 | Opus | Native build (NDK, SDL's Android project, OpenGL ES 2 path that already exists) for 64-bit ARM, as an APK built in CI and signed with the project's own key. The key is created by the project owner and stored as a CI secret, never in the repository. | APK installs and reaches the picker on an emulator in CI |
| R7.2 | Opus | **ROMs on Android:** files are chosen with the system file picker (single files, zips or a whole folder) and scanned by CRC as on desktop. No broad storage permission. | Import a zip on the emulator; game becomes available |
| R7.3 | Sonnet | **Touch controls:** on-screen stick and three buttons plus start and coin, movable and resizable, with opacity setting, hidden automatically when a controller connects. Menus already take touch from U1.3. | Screenshots; layout saved and restored |
| R7.4 | Sonnet | Controllers over Bluetooth and USB, including phone clip pads; several controllers for local multiplayer; TV boxes with a remote can at least navigate the menus. | Scripted input tests |
| R7.5 | Opus | **Lifecycle:** pause and silence on switching away; state kept when the system reclaims the app (uses N1); online, leaving the app drops the seat cleanly with the N4.7 reconnect window instead of freezing the table. | Background and foreground on the emulator, offline and in `nettest` |
| R7.6 | Opus | **Performance:** full speed on a mid-range phone with the simple shaders; CRT shaders stepped down automatically; battery saver frame pacing; audio latency measured. | Frame time log on two device classes |
| R7.7 | Opus | **Online on Android:** Wi-Fi and mobile data, network change mid-game handled, R5.2 green on ARM64, a phone against Windows, macOS and Linux. | Four-platform game in `nettest`, equal hashes |
| R7.8 | Sonnet | Phone layouts of the Part 2 and Part 4 screens: portrait refused or adapted, text sizes, safe areas around notches, on-screen keyboard instead of the system one where it fits. | Screenshots at three phone sizes |

## Phase R8: first full release

| ID | Model | Task |
|---|---|---|
| R8.1 | Opus | Cross-play matrix: every pair of Windows, Linux, macOS and Android plays a full game, and a four-player table with one of each, with equal hashes. Rooms list a mixed set of platforms; a platform icon shows next to each name. |
| R8.2 | Sonnet | One tag builds everything: Windows installer and zip, Linux AppImage and archive, macOS disk image, Android APK, checksums, and the release page from R2.2. |
| R8.3 | Haiku | README, in-app help and the online guide checked against the released build on each platform, in both languages. |

**Gate:** advisor sign-off on the matrix, then publish.

## Limits to expect

* **macOS will warn that the developer is unidentified.** Removing that warning needs an
  Apple Developer membership, which is a yearly fee. Without it, users approve the app
  once in System Settings; recent macOS versions make this a few more clicks than before.
* **Windows may show a SmartScreen warning** on an unsigned installer until R3.3 lands
  a free signing route, if one is granted.
* **Android is sideload only.** A store listing costs a one-time fee and brings review
  policies that a ROM-dependent fan project may not pass. Users enable installs from
  the browser once.
* **iOS is not included.** It has no free route for distributing a native app.
* **Phones on mobile data are usually behind strict NAT,** so they will be relayed through
  a room member more often than PCs (N2.3), and Wi-Fi is the realistic way to play.
* **Cross-play rests on R5.** One platform-dependent calculation anywhere in the
  emulation shows up as desyncs between a phone and a PC and nowhere else.
* **Touch controls are a compromise** for a game built around a stick and turbo button; a
  controller is the intended way to play on Android.
