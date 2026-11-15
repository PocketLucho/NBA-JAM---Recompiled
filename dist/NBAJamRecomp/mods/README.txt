NBA JAM Recomp - Lua mods
=========================

Every .lua file in this folder appears in the main menu under "Lua mods".
Left/Right turns a mod on or off. Enter opens its page:
  * Apply to: All games / Selected games (then tick the games you want)
  * the mod's own options
Settings are saved in mods.ini next to the program.

Writing a mod
-------------
Lua 5.4 with the base, string, table, math and utf8 libraries (no io/os/require/load).
Declare the mod in a global table, then use the hooks:

  mod = {
    name = "My mod",
    description = "Shown in the mod menu.",
    games = { "nbajamte", "nbajamse" },   -- optional; omit = any game
    options = {
      { id = "speed", label = "Speed", values = { 1, 2, 3 },
        labels = { "Slow", "Normal", "Fast" }, default = 2 },  -- default is a value
    },
  }

  function on_load()      -- game loaded, before reset: patch the program ROM here
  end
  function on_frame(n)    -- after every emulated frame (n = frame number)
  end

Game ids: nbajam, nbajamte, nbajamse (T-Unit), nbahangt, nbamht (Wolf Unit).

The nba table
-------------
Addresses are TMS34010 *bit* addresses (byte address * 8), as in the game source.
The program ROM is at nba.ROM_BASE (0xFF800000); a word is 16 bits = 0x10 apart.

  nba.game                     id of the running game
  nba.option(id)               current value of one of your options
  nba.log(...)                 message on screen and on stderr
  nba.frame()                  frames since the game started
  nba.rom_size()               program ROM size in 16-bit words
  nba.rom_read16(a) / nba.rom_write16(a, v)
  nba.rom_read32(a) / nba.rom_write32(a, v)     low word first, like the CPU
  nba.rom_find({w1, w2, -1, ...} [, start [, max]])
                               list of addresses where the word pattern occurs;
                               -1 matches any word
  nba.read8/16/32(a), nba.write8/16/32(a, v)
                               live memory (RAM, I/O) as the main CPU sees it

Each instruction of the original source assembles to fixed words (e.g. "movi X,a0" is
09E0 followed by X, low word first), so a mod can find the code it wants by pattern
and works on every revision that shares that code. See quarter_length.lua and
no_select_timer.lua.

Online games: the program ROM is part of the netplay fingerprint, so all players need
the same mods turned on with the same options.

Online play
-----------
For now, an online game does not start while a mod that has an on_frame hook or that
writes RAM (nba.write8 / write16 / write32) is on for that game: the players could not be
kept in sync. Mods that only patch the ROM in on_load work online; every player must
have the same ones on with the same options. ("Quarter length" has an on_frame hook for
quarters longer than 9 minutes, so it has to be off online.)
