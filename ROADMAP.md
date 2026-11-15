# NBA JAM Recomp roadmap

This file is the working plan for the project. Part 1 (Lua scripts) is specified below;
later parts are added as they are agreed. Each task names the model that should do it:

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
4. **Every user-visible string exists in Spanish and English.**
5. **Each script declares the games it supports**, and is tested on every one it declares.
6. **Tournament mode wins.** When a game's own tournament mode is on, scripts that grant
   power-ups or secret characters stay off, as the original does.
7. One pull request per task or small group of tasks, as a draft, with the test output.

## Where things stand

* `main` has no scripting. Pull request
  [richvam/NBA-JAMs-Recomp#2](https://github.com/richvam/NBA-JAMs-Recomp/pull/2) (draft,
  not merged) adds Lua 5.4.6, the `nba` API, the **Lua mods** menu, `mods.ini`,
  `tools/headless/modtest`, and two scripts: `quarter_length.lua` and `no_select_timer.lua`.
* Pull request [richvam/NBA-JAMs-Recomp#1](https://github.com/richvam/NBA-JAMs-Recomp/pull/1)
  (draft) adds controller remapping. Both touch `src/app/main.cpp` and the Makefile.
* Limits of the engine in pull request 2 that Part 1 has to remove:
  * Options are value lists only. There is no plain on/off switch, no per-player choice,
    no grouping, and no way to change anything while a game is running.
  * Scripts cannot share code (no `require`), so every script would repeat the same
    address lookups and game-state checks.
  * No named symbols. Scripts find addresses by opcode pattern only.
  * The netplay fingerprint covers the program ROM only. Two players with different
    scripts that write RAM pass the lobby check and desync later.
  * No input injection, no on-screen drawing, no events (game start, quarter, game end).

---

# Part 1: Lua scripts

## Phase 0: land the foundation

| ID | Model | Task | Check |
|---|---|---|---|
| 0.1 | Sonnet | Rebase pull request 2 on `main`, resolve conflicts, get it ready to merge. Then rebase pull request 1 on top. | Linux app and `modtest` build; both existing scripts still pass their headless runs on T.E. 4.0 and SE 5.02 |
| 0.2 | Sonnet | Add a CI job that builds `libcore.a`, the Linux app and `modtest`, and runs the Lua engine tests that need no ROM (sandbox, budget, option parsing). | Job green on a pull request |
| 0.3 | Opus | Interim netplay guard: while netplay is active, refuse to start if any enabled script has an `on_frame` hook or writes RAM, with a clear message. Removed again in 1.7. | `nettest` with a RAM-writing mod enabled refuses to start |

**Gate:** pull request 2 merged; CI green.

## Phase 1: engine extensions

All Opus unless marked. Each one extends `src/app/mods.{h,cpp}` and `mods/README.txt`.

| ID | Task | Check |
|---|---|---|
| 1.1 | **Shared libraries.** `nba.include("lib/name.lua")`, limited to `mods/lib/`, no path escapes. Files in `lib/` are not listed as mods. | A mod using a lib runs; `../` is rejected |
| 1.2 | **Symbols.** `nba.sym("name")` returns the bit address for the running revision, or nil. Source-built games: `tools/gspasm` already writes a `.map`; add `tools/map2sym.py` to turn it into `mods/sym/<game>_<program CRC32>.lua` (names and addresses only). ROM-only games: the same file format, filled by hand in Phase 2, each entry carrying the opcode pattern that proves it. | `nba.sym("scores")` equals the `.map` value on T.E. 4.0, SE 5.02 and Maximum Hangtime 1.3 |
| 1.3 | **Option types.** Add `type = "toggle"`, `type = "players"` (a tick per player 1 to 4, stored as a bit mask), `type = "range"` (min, max, step), and `group = "..."` headings. Options can declare `conflicts = { ids }`; the menu greys out the loser. Existing value-list options keep working. | Menu screenshot under Xvfb; `mods.ini` round-trips every type |
| 1.4 | **In-game mods menu.** A key (and pad button) opens the mods page over the running game, offline only. Options marked `live = true` apply at once through an `on_option(id, value)` hook; the others say "applies next game". | Toggle a live option mid-game in `modtest` and see the write |
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
| 2.4 | Opus | **NBA JAM 1993 rev 3.01** (no source). Start from the T.E. patterns for shared code, then disassemble. Slowest item in Part 1; needs the user's `nbajam.zip` in the session. |
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

`fantasy_draft.lua` is the largest script in Part 1 and comes after everything above,
because it depends on the picker from Phase 4, the events from 1.5 and netplay sync from 1.7.

| ID | Model | Task | Check |
|---|---|---|---|
| 6.F1 | Opus | Trace how a selected team becomes two players on court: the roster tables, attribute tables, head and body art pointers, name plates and announcer name calls. Decide what can be mixed freely (any two players on one side) and what cannot (for example uniforms and team name on the scoreboard). | Written note; a hand-made mixed pair plays a headless quarter with correct heads, names and stats |
| 6.F2 | Opus | Draft screen on the frontend overlay before team select: 2 to 4 drafters, snake order, a pick timer, the full player pool with attributes, hidden characters included when `unlock_players.lua` allows them. Picks are made with the normal controls. | Scripted four-player draft ends with four distinct legal picks |
| 6.F3 | Sonnet | Options: draft order (random, loser picks first, fixed), pool (current rosters, expanded rosters, everyone), no duplicate players, and saved draft results that can be replayed. Port to every game. | Headless on each supported game |
| 6.F4 | Opus | Netplay: the draft runs in lockstep as part of the game's input stream, so all peers see the same picks. | `nettest` draft with four peers, equal hashes |

Each script gets the same treatment as Phase 3: symbols from the table, a headless
expectation, both languages, the README list updated by Haiku.

## Phase 7: release

| ID | Model | Task |
|---|---|---|
| 7.1 | Sonnet | Package `mods/`, `mods/lib/` and `mods/sym/` in the Windows and Linux builds and in `release.yml` |
| 7.2 | Haiku | `README.md`, `README.es.md`, `dist/NBAJamRecomp/README.txt`, `LEEME.txt`, `mods/README.txt`: script list, API reference, netplay behaviour |
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

# Part 2 and later

To be added.
