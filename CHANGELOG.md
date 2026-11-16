# Changelog

What changed in each version of NBA JAM Recomp, written for players.

**Rule:** every pull request adds at least one line under "Unreleased", in the section it
belongs to. CI checks this (`tools/ci/check_changelog.py`). A pull request that changes
nothing a player or modder would notice (CI, refactoring, the roadmap) is marked with the
`no-changelog` label instead. At release time "Unreleased" becomes the release page.

Sections, in this order: **Added**, **Changed**, **Fixed**, **Online**, **Known issues**.

## Unreleased

### Added
- Lua mods: a "Lua mods" menu, per-game settings in `mods.ini`, and two scripts: quarter
  length and no team-select timer.
- Controller remapping: profiles per player and per game, button shortcuts, import and export.
- The program shows its version, build and data folder in About, and its version on the
  main menu. `NBAJamRecomp --version` prints it.
- Your files can live in your user folder (`%APPDATA%\NBAJamRecomp` on Windows,
  `~/.local/share/NBAJamRecomp` on Linux), so the program can be installed where it cannot
  write. The zip and tar packages stay portable: they ship with `portable.txt` and keep
  everything in their own folder, as before.
- A copy without `portable.txt` that finds an older setup next to the program offers to
  move it.
- This changelog.
- Release pages are written from this changelog: what changed, which download is for which
  system, first-run steps, and whether the version plays online with the previous one.
- News: the main menu tells you when a newer version has been published and the News screen
  shows its release notes. It only informs you; nothing is downloaded or installed. Turn it
  off with Left/Right on "News and updates" or `news=0` in `nbajam_recomp.ini`.
- About shows the latest release and where to get it.
- Side fill (F8): beside a 4:3 picture on a wider screen you can have black, the picture
  itself blurred and dimmed, or your own image (`bezel.png` in the data folder).

### Changed
- The CRT mask is as coarse at 4K as at 1080p (it was twice as fine at 4K), and it lines up
  with the picture wherever the picture sits on the screen.
- Windows: the program draws at the monitor's real resolution when display scaling is set to
  150% or 200%, instead of being enlarged by the system. A new window is sized from the
  screen (about three quarters of its height) instead of always 1200x900; `scale=` in the
  settings file still sets a fixed size.
- Scaling (F5) is now Fit, Smooth or Integer, and **Fit is the default**: the picture fills
  the height of the screen with crisp pixels, so there is no border above and below at
  1080p or 4K. Integer is still there and says so when it leaves a large part of the screen
  unused. An `integer_scale=` line in an older settings file is not carried over.
- Shape (F7) is now Original 4:3 or Stretch. Stretch is the old 16:9, 16:10 and fill modes
  under an honest name, and works on any monitor shape, ultrawide included. There is no true
  widescreen view yet (see Known issues).
- The interface and the documentation are English only for now. The language option is gone
  from the main menu.
- Downloads are on the Releases page; builds are no longer kept in the repository.
- The README is rewritten for players: download, where the ROMs go, first run, controls,
  online, mods and troubleshooting. Build instructions moved to `docs/BUILDING.md`.
- The version is taken from the release tag instead of the build date.

### Fixed

### Online
- An online game does not start while a Lua mod that writes memory or runs every frame is
  switched on, because the players could not be kept in sync. Mods that only patch the game
  at load still work if every player has the same ones on. "Quarter length" is affected and
  has to be off online.

### Known issues
- No true widescreen. The games draw a 4:3 picture; on a wide screen you get that picture
  with sides (black, blurred or your own image, F8) or stretched (F7). Showing more of the
  court needs changes to each game and is planned, starting with Tournament Edition.
- "No team select timer" removes the countdown on one of the two screens it looks for on
  T.E. and T.E. Special Edition.
- Long mod descriptions are cut off in the Lua mods list.
