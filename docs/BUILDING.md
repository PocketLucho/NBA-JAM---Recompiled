# Building NBA JAM Recomp

For players, the [README](../README.md) is the place to start. This page is for building
the program, running its tests, and rebuilding the games from their source code.

## Repository layout

| Path | Contents |
|---|---|
| `src/emu` | minimal MAME-compatible device framework |
| `src/mame` | unmodified MAME CPU cores (TMS34010, M6809, ADSP-2105), OKI ADPCM, ymfm |
| `src/board` | Midway T-Unit (DMA blitter, protection, Williams ADPCM sound) and Wolf Unit (security PIC, DCS) |
| `src/app/ui` | the menu toolkit: widgets, navigation, unified input (no SDL inside; `ui_sdl.h` is the adapter) |
| `src/app` | SDL2 application: menu, ROM loading, video, shaders, netplay, Lua mods, data folder, release news |
| `src/third_party/lua` | Lua 5.4.6, unmodified |
| `tools/ci` | the build, packaging and check scripts that CI runs |
| `tools/headless` | command-line test programs |
| `tools/gspasm` | assembler/linker compatible with TI GSPA/GSPLNK, written for this project |
| `tools/build_roms.sh` | rebuilds the games from the source repositories |
| `tools/lod_palbpp.py`, `fix_narrow.py`, `fix_pwrd.py` | make the 1995 LOAD2 output match the 1994 tools (see below) |
| `tools/irw2gfx*.py`, `imgfile.py` | LOAD2 image file helpers |
| `tools/emu2-ems.patch` | EMS support for [emu2](https://github.com/dmsc/emu2), needed by LOAD2.EXE |
| `dist/NBAJamRecomp` | what goes into a package besides the executable: read-me, mods, empty `roms` folder |

## Building the program

The supported way is the one CI uses, on Ubuntu (or WSL on Windows). It builds both the
Linux program and, by cross-compiling with mingw-w64, the Windows one.

```sh
sudo apt-get install build-essential mingw-w64 zip \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
  libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev libdrm-dev libgbm-dev libgl-dev

# SDL2 2.30.9, downloaded with a pinned checksum
tools/ci/fetch_sdl.sh linux deps/sdl2linux      # static library built from source
tools/ci/fetch_sdl.sh win   deps/sdl2win        # official MinGW package

tools/ci/build_all.sh linux deps/sdl2linux build/NBAJamRecomp-linux
tools/ci/build_all.sh win   deps/sdl2win   build/NBAJamRecomp.exe

# optional: the same archives a release contains
tools/ci/package.sh linux   build/NBAJamRecomp-linux NBAJamRecomp-dev-linux-x64   out
tools/ci/package.sh windows build/NBAJamRecomp.exe   NBAJamRecomp-dev-windows-x64 out
```

The Makefile targets behind those scripts are `linux-app` (`SDL2L=<dir>`) and `win`
(`SDL2W=<dir>`), plus `build/<target>/libcore.a` and `liblua.a`.

The version shown by the program comes from the nearest `v*` git tag
(`make print-version`); outside a git checkout it is `dev`.

## Tests

None of these need a ROM; CI runs them on every pull request.

| Program | Checks |
|---|---|
| `build/linux/luatest` | the Lua mod engine: options, sandbox, instruction budget, ROM patch API, hooks, `mods.ini`, the netplay guard |
| `build/linux/uitest [dir]` | the menu toolkit: the automated walk of the navigation rule, every input type, repeat timing, prompts, text fitting; the project lettering, the themes and the fallback for characters a game font lacks, and the rebuilt screens: game picker, Settings, Controls, Lua mods, Online, About and help, News (the same walk, and their PNGs); with a folder, PNGs of the widget gallery and `font_sheet.png`, the full character sheet. `viewtest --gallery <dir>` draws it through the presenter at four sizes, and `NBAJamRecomp --ui-gallery` opens it in the program |
| `build/linux/arttest [dir]` | the ROM art reader on a made-up ROM built in memory: lettering, logos, backgrounds and compressed pictures decoded pixel for pixel, and the fallback for wrong, cut or missing ROMs. `arttest --roms <app folder> [dir]` reads your own ROMs (`<app folder>/roms`) and lists what it finds per game; with a folder it writes the pages of the test screen as PNGs. It also writes the gallery in each game's theme. Those hold game art, so keep that folder outside the repository. `NBAJamRecomp --rom-art` shows the same pages in the program |
| `build/linux/pathtest` | the data folder: portable and per-user modes, first-run seeding, moving an older setup |
| `build/linux/viewtest --rects` | where the picture goes in the window for every shape, scaling and listed resolution |
| `xvfb-run -a -s "-screen 0 5120x2160x24" build/linux/viewtest --all <dir>` | draws a test pattern through the real presenter at every listed resolution, checks the result and writes PNGs. `--shaders <dir>` does the same for scanlines, CRT and curved CRT at 1080p, 1440p, 4K and 3440x1440, and `--fills <dir>` for the three side fills. One frame: `viewtest <w> <h> <shape> <scaling> <shader> <out.png>` |
| `build/linux/newstest` | release news: reading the feed answer, version comparison, plain-text notes |
| `python3 tools/ci/check_repo.py` | no ROMs, archives or binaries are tracked |
| `python3 tools/ci/check_changelog.py origin/main` | the pull request adds a line to `CHANGELOG.md` |
| `python3 tools/ci/release_notes.py --self-test` | the release page generator |

With your own ROMs:

| Program | Use |
|---|---|
| `build/linux/modtest prog_lo prog_hi mods_dir game_id [frames] [opt=value]` | runs the Lua mods on a program ROM and prints what they patched. `PLAY=1` presses start and shoot to reach a game; `WATCH=<address>` prints a memory value every 500 frames |
| `build/linux/nettest <base> <game> host <port> <frames> <delay>` and `... client <ip> <port> <frames>` | one side of an online game with scripted inputs; prints state hashes to compare. `MODS=<dir>` switches on the mods in a folder |

Never commit ROMs or anything built from them; `check_repo.py` refuses them.

## Releasing

1. `python3 tools/ci/release_notes.py cut <version>` turns "Unreleased" in `CHANGELOG.md`
   into that version's section. Commit it.
2. Tag the commit `v<version>` and push the tag.
3. CI builds both packages and creates a **draft** release with them, `SHA256SUMS.txt` and
   the release page written from the changelog. Review it and publish.

## Building the games from source

Needs bash, python3 and emu2 with `tools/emu2-ems.patch` (Linux, or WSL on Windows):

```sh
git clone https://github.com/historicalsource/nba-jam-tournament-edition repos/nba-jam-tournament-edition
git clone https://github.com/Asure/nba-jam-tournament-edition-SE        repos/nba-jam-tournament-edition-SE
git clone https://github.com/historicalsource/nba-hangtime              repos/nba-hangtime
tools/build_roms.sh repos /path/to/emu2 <program folder>/data
```

The script assembles and links every game with `gspasm`, runs Midway's `LOAD2.EXE` to
pack the art, compares the results with the original ROMs and writes `data/<game>/`. The
program uses `data/<game>/` when it does not find that game's ROMs in `roms/`.

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
