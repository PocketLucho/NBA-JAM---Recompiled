-- quarter_length.lua - choose how long each quarter (and overtime) lasts.
--
-- The game keeps its clock in game_time as four packed bytes:
--   minutes << 24 | tens of seconds << 16 | seconds << 8 | tenths
-- and loads the start value from the constant QRTRTIME (3:00 in T.E., "[300h,0h]" in
-- GAME.EQU) with "movi QRTRTIME,a0" before "move a0,@game_time,L". We look those
-- instructions up in the program ROM and change the constant, so it works on any
-- revision that shares that code.

local lengths = { 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9 }
local labels = {}
for i, m in ipairs(lengths) do
  labels[i] = string.format("%d:%02d", math.floor(m), math.floor((m % 1) * 60 + 0.5))
end

mod = {
  name = "Quarter length",
  description = "Sets the minutes per quarter (overtime uses the same length). The original is 3:00.",
  games = { "nbajam", "nbajamte", "nbajamse" },
  options = {
    { id = "minutes", label = "Quarter length", values = lengths, labels = labels, default = 3 },
  },
}

-- packed clock value for a length in minutes
local function clock_value(m)
  local mins = math.floor(m)
  local secs = math.floor((m - mins) * 60 + 0.5)
  return (mins << 24) | ((secs // 10) << 16) | ((secs % 10) << 8)
end

function on_load()
  local minutes = nba.option("minutes")
  local value = clock_value(minutes)

  -- movi 0x03000000,a0   (opcode 09E0, 32-bit immediate low word first)
  local hits = nba.rom_find({ 0x09E0, 0x0000, 0x0300 })
  if #hits == 0 then
    nba.log("quarter clock constant not found in this game's ROM; nothing changed")
    return
  end

  -- game_time = the operand of "move a0,@addr,L" (0780 lo hi) right after the load
  local game_time
  for _, a in ipairs(hits) do
    if nba.rom_read16(a + 3 * 16) == 0x0780 then game_time = nba.rom_read32(a + 4 * 16); break end
  end
  if not game_time then
    nba.log("could not identify the game clock; nothing changed")
    return
  end

  local patched = 0
  for _, a in ipairs(hits) do
    local next_op = nba.rom_read16(a + 3 * 16)
    local stores_clock = next_op == 0x0780 and nba.rom_read32(a + 4 * 16) == game_time
    -- regulation quarter start: "move @gmqrtr,a1" follows and the value is stored a bit later
    local quarter_start = next_op == 0x05A1
    if stores_clock or quarter_start then
      nba.rom_write32(a + 16, value)
      patched = patched + 1
    end
  end
  nba.log(string.format("quarters set to %d:%d%d (%d places patched, clock at 0x%08X)",
    value >> 24, (value >> 16) & 0xff, (value >> 8) & 0xff, patched, game_time))
end
