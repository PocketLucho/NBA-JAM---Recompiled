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

### Changed
- The interface and the documentation are English only for now. The language option is gone
  from the main menu.
- Downloads are on the Releases page; builds are no longer kept in the repository.
- The version is taken from the release tag instead of the build date.

### Fixed

### Online
- An online game does not start while a Lua mod that writes memory or runs every frame is
  switched on, because the players could not be kept in sync. Mods that only patch the game
  at load still work if every player has the same ones on. "Quarter length" is affected and
  has to be off online.

### Known issues
- "No team select timer" removes the countdown on one of the two screens it looks for on
  T.E. and T.E. Special Edition.
- Long mod descriptions are cut off in the Lua mods list.
