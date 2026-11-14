-- quarter_length.lua - choose how long each quarter (and overtime) lasts.
--
-- The game keeps its clock in game_time as four packed bytes:
--   minutes << 24 | tens of seconds << 16 | seconds << 8 | tenths
-- and loads the start value from the constant QRTRTIME (3:00, "[300h,0h]" in GAME.EQU)
-- with "movi QRTRTIME,a0" before "move a0,@game_time,L". We look those instructions up
-- in the program ROM and change the constant, so it works on every game of the saga
-- that shares that code (NBA JAM T.E., T.E. SE, Hangtime and Maximum Hangtime).
--
-- Over 9 minutes: the scoreboard has a single minutes digit (its digit table only has
-- 0-9), so the ROM starts the clock at 9:00 and this script keeps the extra minutes:
-- each time the clock would go from 9:00 to 8:59 while extra minutes remain, it puts
-- the minutes back to 9. The time played is exact; the minutes digit reads 9 until
-- there are 9 minutes left.

local lengths = { 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }
local labels = {}
for i, m in ipairs(lengths) do
  labels[i] = string.format("%d:%02d", math.floor(m), math.floor((m % 1) * 60 + 0.5))
end

mod = {
  name = "Quarter length",
  description = "Sets the minutes per quarter, up to 12 (overtime uses the same length). The original is 3:00. Over 9:00 the minutes digit shows 9 until 9 minutes are left.",
  games = { "nbajam", "nbajamte", "nbajamse", "nbahangt", "nbamht" },
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

local game_time        -- address of the clock, found in on_load
local extra_per_quarter = 0
local extra = 0        -- extra whole minutes left in this quarter
local prev_min = -1

function on_load()
  local minutes = nba.option("minutes")
  extra_per_quarter = math.max(0, math.floor(minutes) - 9)
  local value = clock_value(math.min(minutes, 9))

  -- movi 0x03000000,a0   (opcode 09E0, 32-bit immediate low word first)
  local hits = nba.rom_find({ 0x09E0, 0x0000, 0x0300 })
  if #hits == 0 then
    nba.log("quarter clock constant not found in this game's ROM; nothing changed")
    return
  end

  -- game_time = the operand of "move a0,@addr,L" (0780 lo hi) right after the load
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
  nba.log(string.format("quarters set to %s (%d places patched, clock at 0x%08X)",
    labels[(function() for i, m in ipairs(lengths) do if m == minutes then return i end end return 1 end)()],
    patched, game_time))
end

function on_frame()
  if extra_per_quarter == 0 or not game_time then return end
  local t = nba.read32(game_time)
  local min = t >> 24
  if t == 0x09000000 and prev_min ~= 9 and prev_min ~= 8 then
    -- a quarter (or overtime) has just started at 9:00
    extra = extra_per_quarter
  elseif prev_min == 9 and min == 8 and extra > 0 then
    -- 9:00 -> 8:59.9: one of the extra minutes has gone by, back to 9:59.9
    nba.write8(game_time + 24, 9)
    extra = extra - 1
    min = 9
  end
  prev_min = min
end
