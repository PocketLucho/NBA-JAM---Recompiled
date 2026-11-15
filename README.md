# NBA JAM Recomp

*Castellano: [README.es.md](README.es.md)*

Midway's NBA JAM arcade saga, **rebuilt from its original source code** and played in one
Windows application with a game picker, CRT shaders, widescreen modes and online play.

Created by **Pocket_lucho** with **Claude** (Anthropic).
If you want to support me, go to https://buymeacoffee.com/pocket_lucho

| Game | Board | Status |
|---|---|---|
| NBA JAM (1993), rev 3.01 | T-Unit | runs from the MAME romset (its source code was never published) |
| NBA JAM Tournament Edition, rev 4.0 | T-Unit | **built from source: program and graphics byte-identical to the original ROM** |
| NBA JAM T.E. Special Edition, rev 5.02 (Asure, 2025) | T-Unit | **built from source: program byte-identical** |
| NBA Hangtime, L1.3 / L1.1 | Wolf Unit | runs from the ROMs |
| NBA Maximum Hangtime, 1.3 | Wolf Unit | **built from source: program byte-identical** (1.03 from MAME also runs) |

## Download

Get the latest version from the **[Releases page](https://github.com/richvam/NBA-JAMs-Recomp/releases)**:

* Windows 10/11 x64: `NBAJamRecomp-<version>-windows-x64.zip`
* Linux x64: `NBAJamRecomp-<version>-linux-x64.tar.gz`

Builds are made automatically for every version tag; none are kept in the repository.

Unpack, copy your ROMs into `roms/` (see below) and run `NBAJamRecomp`.

## No game data is included

Midway's code, graphics, sound and music are copyrighted, so neither this repository nor
the releases contain any of them. Each user copies their own files into `roms/`:

* MAME romsets: `nbajam.zip`, `nbajamte.zip`, `nbahangt.zip`, `nbamht.zip`
* and/or the ZIPs of the public source repositories ("Code → Download ZIP"):
  [historicalsource/nba-jam-tournament-edition](https://github.com/historicalsource/nba-jam-tournament-edition),
  [Asure/nba-jam-tournament-edition-SE](https://github.com/Asure/nba-jam-tournament-edition-SE),
  [historicalsource/nba-hangtime](https://github.com/historicalsource/nba-hangtime)

The application scans `roms/` (sub-folders and ZIPs included), recognises every file by
CRC32 and extracts what each game needs when it starts. See `dist/NBAJamRecomp/README.txt`
for the per-game list. Games built from source with `tools/build_roms.sh` (below) go to
`data/<game>/` and are used when the ROMs are not found.

## Features

* Game picker ordered by release date, Spanish / English interface
* OpenGL 2.1 / GLES 2 shaders: sharp, smooth, scanlines, CRT, curved CRT; integer
  (pixel perfect) scaling; 4:3, 16:9, 16:10 or fill
* Online play for up to 4 players: lockstep over UDP, with ROM fingerprint and
  desync detection
* Keyboard (4 players) and gamepads with remappable controls: per-player controller profiles for all games or per game, one-button shortcuts for multi-button moves (shove, turbo shot, custom), importable/exportable as `.ini` files (main menu -> Controls)
* Per-game NVRAM, screenshots (F12)
* Lua mods: `.lua` scripts in `mods/`, switched on from the **Lua mods** menu for all
  games or for chosen ones (see [Lua mods](#lua-mods))

## Lua mods

Scripts in `mods/` next to the program show up in the main menu under **Lua mods**.
Left/Right turns a mod on or off; Enter opens its page, where *Apply to* switches between
**All games** and **Selected games** (one checkbox per game) and the mod's options are set.
Choices are saved in `mods.ini`. Mods run in a sandbox (no file or OS access) with Lua 5.4.

Included:

| Script | What it does |
|---|---|
| `quarter_length.lua` | quarter (and overtime) length from 0:30 to 12:00; the original is 3:00. The scoreboard has one minutes digit, so above 9:00 it reads 9 until 9 minutes are left (the time played is exact) |
| `no_select_timer.lua` | removes the countdown on the team select screen (option: the initials entry one too) |

Both find the code they change by its instruction pattern, so they work on any revision
that shares it. They were checked on T.E. rev 4.0, T.E. SE rev 5.02, Hangtime L1.3 and Maximum Hangtime 1.3
with `build/linux/modtest` (`make build/linux/modtest`): with `quarter_length` set to 1:30
every quarter starts at 1:30 (12:00 checked on T.E.), and with `no_select_timer` the select
screen waits for the players instead of picking for them. Hangtime's shorter countdowns
("winner stays", trivia) are left alone. NBA JAM (1993) has no public source and is
untested; if the pattern is not in its ROM the mod says so on screen and changes nothing.

Online games: both players need the same mods, since they change the program and are
part of the ROM fingerprint. Writing your own: see `dist/NBAJamRecomp/mods/README.txt`.

## Repository layout

| Path | Contents |
|---|---|
| `src/emu` | minimal MAME-compatible device framework |
| `src/mame` | unmodified MAME CPU cores (TMS34010, M6809, ADSP-2105), OKI ADPCM, ymfm |
| `src/board` | Midway T-Unit (DMA blitter, protection, Williams ADPCM sound) and Wolf Unit (security PIC, DCS) |
| `src/app` | SDL2 application: menu, ROM loading, video, shaders, netplay, Lua mods |
| `src/third_party/lua` | Lua 5.4.6, unmodified |
| `tools/gspasm` | assembler/linker compatible with TI GSPA/GSPLNK, written for this project |
| `tools/build_roms.sh` | rebuilds the games from the source repositories |
| `tools/lod_palbpp.py`, `fix_narrow.py`, `fix_pwrd.py` | make the 1995 LOAD2 output match the 1994 tools (see below) |
| `tools/irw2gfx*.py`, `imgfile.py` | LOAD2 image file helpers |
| `tools/emu2-ems.patch` | EMS support for [emu2](https://github.com/dmsc/emu2), needed by LOAD2.EXE |
| `tools/headless` | command-line test programs |
| `dist/NBAJamRecomp` | release folder (docs; the .exe is built here) |

## Building the application

Windows build (cross-compiled from Linux with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
and SDL2 2.30 for mingw):

```sh
make BUILD=build/win CXX=x86_64-w64-mingw32-g++ build/win/libcore.a
make win SDL2W=/path/to/SDL2-2.30.x/x86_64-w64-mingw32     # -> dist/NBAJamRecomp/NBAJamRecomp.exe
```

Linux build: `make linux-app SDL2L=/path/to/static-sdl2` → `build/linux/NBAJamRecomp` (x86-64, glibc 2.38+; SDL2 and libstdc++ linked statically).

## Building the games from source

Needs bash, python3 and emu2 with `tools/emu2-ems.patch` (Linux, or WSL on Windows):

```sh
git clone https://github.com/historicalsource/nba-jam-tournament-edition repos/nba-jam-tournament-edition
git clone https://github.com/Asure/nba-jam-tournament-edition-SE        repos/nba-jam-tournament-edition-SE
git clone https://github.com/historicalsource/nba-hangtime              repos/nba-hangtime
tools/build_roms.sh repos /path/to/emu2 dist/NBAJamRecomp/data
```

The script assembles and links every game with `gspasm`, runs Midway's `LOAD2.EXE` to
pack the art, compares the results with the original ROMs and writes `data/<game>/`.

### What it took to get identical ROMs

* **gspasm** reproduces GSPA's quirks: TI operator precedence (`| ^` like `+ -`, `& << >>`
  like `*`), surplus macro arguments folded into the last parameter, and GSPLNK's
  `0x0000FFFF` hole filling.
* **T.E. art packing** - the T.E. was packed in 1994 by an older LOAD2 that gave every
  image the bits per pixel of its palette, while the 1995 LOAD2 in the sources packs to the
  minimum. `lod_palbpp.py` adds `PPP> n` lines to the LOD files (otherwise name plates
  are noise).
* **Narrow images** - the 1995 LOAD2 pads images narrower than 3 pixels; `fix_narrow.py`
  removes the padding column and moves all later image addresses back.
* **Second animation point** - the 1995 LOAD2 writes `-1` to the `PWRD1..3` header fields
  (where the ball is held); `fix_pwrd.py` takes them from the `.IMG` files. Without it the
  ball floats and never bounces.

## Licence

The code of this project is released under the BSD 3-Clause licence (see `LICENSE`).
Third-party code: MAME CPU cores and ymfm (BSD 3-Clause), miniz (MIT / Unlicense),
font8x8 (public domain), SDL2 (zlib), Lua 5.4 (MIT). NBA JAM, NBA Hangtime and Midway are trademarks of
their owners; this is a non-profit fan project not affiliated with them.

## Credits

* Midway Games: the original games and the source code released by
  [historicalsource](https://github.com/historicalsource)
* [Asure](https://github.com/Asure): NBA JAM T.E. Special Edition and its sources
* The MAME team, Aaron Giles (ymfm), dmsc (emu2)
