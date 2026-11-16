# NBA JAM Recomp

Midway's NBA JAM arcade games, **rebuilt from their original source code**, in one program
for Windows and Linux: a game picker, CRT shaders, remappable controllers, Lua mods and
online play for up to four players.

Created by **Pocket_lucho** with **Claude** (Anthropic).
If you want to support me, go to https://buymeacoffee.com/pocket_lucho

| Game | Board | Status |
|---|---|---|
| NBA JAM (1993), rev 3.01 | T-Unit | runs from the MAME romset (its source code was never published) |
| NBA JAM Tournament Edition, rev 4.0 | T-Unit | **built from source: program and graphics byte-identical to the original ROM** |
| NBA JAM T.E. Special Edition, rev 5.02 (Asure, 2025) | T-Unit | **built from source: program byte-identical** |
| NBA Hangtime, L1.3 / L1.1 | Wolf Unit | runs from the ROMs |
| NBA Maximum Hangtime, 1.3 | Wolf Unit | **built from source: program byte-identical** (1.03 from MAME also runs) |

**No game data is included.** Midway's code, graphics, sound and music are copyrighted, so
neither this repository nor the downloads contain any of them. You bring your own files
(see [Where the ROMs go](#2-where-the-roms-go)).

Contents: [Download](#1-download) · [Where the ROMs go](#2-where-the-roms-go) ·
[First run](#3-first-run) · [Controls](#4-controls) · [Online](#5-online) ·
[Lua mods](#6-lua-mods) · [Troubleshooting](#7-troubleshooting) ·
[Building it yourself](docs/BUILDING.md) · [What changed](CHANGELOG.md)

## 1. Download

Get the latest version from the **[Releases page](https://github.com/richvam/NBA-JAMs-Recomp/releases)**.

| System | File |
|---|---|
| Windows 10/11, 64-bit | `NBAJamRecomp-<version>-windows-x64.zip` |
| Linux, 64-bit (x86-64) | `NBAJamRecomp-<version>-linux-x64.tar.gz` |

Unpack it anywhere. There is nothing to install. Windows may show a SmartScreen warning
because the program is not signed: choose "More info", then "Run anyway".

## 2. Where the ROMs go

Copy your own files into the `roms` folder next to the program. The program recognises
every file by its contents (CRC32), whatever its name, inside zips and sub-folders too,
so there is nothing to unpack or rename.

| Game | What to put in `roms` |
|---|---|
| NBA JAM (1993) | MAME romset `nbajam.zip` |
| NBA JAM Tournament Edition | MAME romset `nbajamte.zip`, or the ZIP of the [historicalsource/nba-jam-tournament-edition](https://github.com/historicalsource/nba-jam-tournament-edition) repository (without `nbajamte.zip` it uses the Special Edition sound ROMs) |
| NBA JAM T.E. Special Edition | the ZIP of the [Asure/nba-jam-tournament-edition-SE](https://github.com/Asure/nba-jam-tournament-edition-SE) repository |
| NBA Hangtime | MAME romset `nbahangt.zip` (L1.3 or L1.1) |
| NBA Maximum Hangtime | version 1.3: the ZIP of the [historicalsource/nba-hangtime](https://github.com/historicalsource/nba-hangtime) repository plus `nbahangt.zip` (graphics and sound); or version 1.03: MAME romset `nbamht.zip` |

Repository ZIPs are downloaded on GitHub with "Code", then "Download ZIP".

## 3. First run

1. Run `NBAJamRecomp` (`NBAJamRecomp.exe` on Windows).
2. Pick a game with the arrow keys and Enter, or with a controller. A game shown in grey
   is missing files; selecting it tells you which.
3. Press `5` to insert a coin and `1` to start (Back and Start on a controller). `F1`
   shows the keys at any time, and `Esc` returns to the menu.

The downloads are **portable**: settings, saves, screenshots, controller profiles, mods and
ROMs all stay in the unpacked folder, because it contains a file called `portable.txt`.
Without that file the program keeps them in your user folder instead
(`%APPDATA%\NBAJamRecomp` on Windows, `~/.local/share/NBAJamRecomp` on Linux). The About
screen shows which folder is in use.

Updating: unpack the new version over the old folder, or into a new one and copy `roms`,
`saves` and `nbajam_recomp.ini` across. The main menu tells you when a newer version has
been published (**News and updates**; it only tells you, nothing is downloaded, and it can
be switched off there).

## 4. Controls

Keyboard:

| | Move | Shoot | Pass | Turbo | Start | Coin |
|---|---|---|---|---|---|---|
| Player 1 | arrow keys | Z | X | C | 1 | 5 |
| Player 2 | W S A D | F | G | V | 2 | 6 |
| Player 3 | numpad 8 2 4 6 | numpad 0 | numpad . | numpad Enter | 3 | 7 |
| Player 4 | I K J L | H | ; | U | 4 | 8 |

Controllers: each pad is one player. By default A shoots, B passes, X or a shoulder button
is turbo, Start starts and Back inserts a coin.

To change them, open **Controls** in the main menu. Each player has a profile for all
games, and any game can be given its own. One button can hold several arcade buttons at
once (shove, turbo shot and two shortcuts of your choice). Profiles can be exported and
imported as `.ini` files.

Other keys:

| Key | Does |
|---|---|
| F1 | on-screen help |
| F2 | test switch (the game's service menu) |
| F3 | reset the game |
| F4 | picture: sharp, smooth, scanlines, CRT, curved CRT |
| F5 | scaling: Fit (fills the height, crisp pixels), Smooth (the same, filtered), Integer (whole-number size, may leave borders) |
| F6 | scanline strength |
| F7 | shape: Original 4:3, Widescreen (4:3 for now; more court on games that get a wide view later), Stretch (fills the window, distorts the picture) |
| F11 or Alt+Enter | fullscreen |
| F12 or Print Screen | screenshot, saved in `screenshots` |
| P | pause |
| - and + | volume |
| 9 | service credit |
| Esc | back to the menu |

Keyboard keys are changed in `nbajam_recomp.ini`.

## 5. Online

Up to four players, each on their own computer.

1. Highlight the game in the menu and press `N` (or Y on a controller).
2. One player chooses **Host a game**, waits in the lobby and presses Enter to start. They
   are player 1.
3. The others choose **Join**, type the host's IP address (Ctrl+V pastes) and press Enter.
   They become players 2, 3 and 4 in the order they arrive.

Good to know:

* Everybody plays with their own player 1 keys or first controller.
* Everybody needs the same ROMs and the same version of the program; you are told if they differ.
* Over the internet the host forwards UDP port 7845 on their router, or everybody joins a
  virtual network (Tailscale, ZeroTier, Radmin VPN).
* Input delay: 3 to 4 on a home network, 5 to 7 over the internet. If the game keeps
  stopping with "Waiting for the network...", raise it.
* If "DESYNC" appears, leave with Esc and host the game again.
* Lua mods: mods that only change the game when it loads work online if every player has
  the same ones switched on with the same options. A mod that writes memory or runs every
  frame stops an online game from starting, and the program names it. Of the two included
  mods, "Quarter length" has to be off online.

## 6. Lua mods

Scripts in the `mods` folder appear in the main menu under **Lua mods**. Left/Right turns a
mod on or off. Enter opens its page, where you choose the games it applies to and set its
options. Your choices are saved in `mods.ini`. Mods cannot touch your files or your system.

Included:

| Script | What it does |
|---|---|
| `quarter_length.lua` | Quarter and overtime length from 0:30 to 12:00 (the original is 3:00). The scoreboard has one minutes digit, so above 9:00 it shows 9 until nine minutes are left. |
| `no_select_timer.lua` | Removes the countdown on the team select screen, and optionally on initials entry. |

Both were checked on T.E. rev 4.0, T.E. Special Edition rev 5.02, Hangtime and Maximum
Hangtime. If a mod cannot find what it needs in a game, it says so on screen and changes
nothing. To write your own, see [`mods/README.txt`](dist/NBAJamRecomp/mods/README.txt).

## 7. Troubleshooting

| Problem | What to do |
|---|---|
| A game is grey | Select it: the program says which files are missing. Check the table in [Where the ROMs go](#2-where-the-roms-go). |
| "CMOS invalid, factory settings restored" the first time | Normal on a game's first start. Press any button. |
| No picture, or a black window | Your graphics driver may lack OpenGL 2.1. Set `opengl=0` in `nbajam_recomp.ini` for the basic mode (no shaders). |
| The picture is too wide | Press F7 until it says "Original 4:3". "Stretch" distorts the picture. |
| Borders on every side | That is Integer scaling, which only grows in whole steps. Press F5 until it says "Fit". |
| A controller does nothing | Open **Controls**: the top line shows the pad the program sees. Select an action and press the button you want. |
| I cannot find my saves or settings | Open **About**: the "Data" line shows the folder. |
| Online: "your ROMs (or Lua mods) are not the same as the host's" | Use the same ROM files and switch the same mods on, with the same options. |
| Online: the others cannot join | The host's UDP port 7845 must be reachable: forward it on the router or use a virtual network. |
| The menu never mentions new versions | News needs an internet connection and "News and updates: on". With no connection it stays quiet. |

Something else? Open an issue on this repository and say which game and which version
(About shows it).

## Licence and credits

The code of this project is released under the BSD 3-Clause licence (see `LICENSE`).
Third-party code: MAME CPU cores and ymfm (BSD 3-Clause), miniz (MIT / Unlicense),
font8x8 (public domain), SDL2 (zlib), Lua 5.4 (MIT).

* Midway Games: the original games, and the source code released by
  [historicalsource](https://github.com/historicalsource)
* [Asure](https://github.com/Asure): NBA JAM T.E. Special Edition and its sources
* The MAME team, Aaron Giles (ymfm), dmsc (emu2)
* The original project: [PocketLucho/NBA-JAM---Recompiled](https://github.com/PocketLucho/NBA-JAM---Recompiled)

NBA JAM, NBA Hangtime and Midway are trademarks of their owners. This is a non-profit fan
project, not affiliated with them.
