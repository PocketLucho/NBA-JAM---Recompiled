-- no_select_timer.lua - remove the arcade countdown on the team selection screen.
--
-- In the original code the select screen starts a countdown process:
--     movi  30*TSEC,a10              ; 09CA <ticks>
--     CREATE CNTDWN_PID,timeout      ; 09C1 <pid>  09E7 <proc>  0D5F <GETPRC>
-- "timeout" shows 9..0 on screen and then sets force_selection, which picks a team for
-- you. The initials entry before it uses the same routine with 25*TSEC. (Hangtime: 28*TSEC
-- for team select, 25*TSEC for initials; its short 5 and 9 second countdowns on the
-- "winner stays" and trivia screens are left alone.)
-- We point those CREATE calls at the process killer (SUCIDE, the target of DIE), so the
-- countdown process ends the moment it starts: no clock, no forced choice.

mod = {
  name = "No team select timer",
  description = "Removes the countdown on the team select screen, so you can take your time. Optionally the initials entry timer too.",
  games = { "nbajam", "nbajamte", "nbajamse", "nbahangt", "nbamht" },
  options = {
    { id = "initials", label = "Initials entry timer", values = { 0, 1 }, labels = { "Keep", "Remove" }, default = 0 },
  },
}

local W = 16   -- one 16-bit word, in bit addresses

-- is this the countdown routine? It starts with
--   clr a0 / move a0,@force_selection / move a10,a11 / SLEEPK 1
-- and ends with
--   movk 1,a0 / move a0,@force_selection / calla DELOBJA8 / jauc SUCIDE
local function inspect(entry)
  if nba.rom_read16(entry) ~= 0x5600 or nba.rom_read16(entry + W) ~= 0x0580 then return nil end
  if nba.rom_read16(entry + 4 * W) ~= 0x4D4B then return nil end
  local fs = nba.rom_read32(entry + 2 * W)
  local tail = nba.rom_find({ 0x1820, 0x0580, fs & 0xffff, fs >> 16, 0x0D5F, -1, -1, 0xC080 }, entry, 1)
  if #tail == 0 or tail[1] - entry > 0x200 * W then return nil end
  return { force_selection = fs, sucide = nba.rom_read32(tail[1] + 8 * W) }
end

function on_load()
  local remove_initials = nba.option("initials") == 1
  local callers = {}
  local max_ticks = 0
  for _, a in ipairs(nba.rom_find({ 0x09CA, -1, 0x09C1, -1, 0x09E7, -1, -1, 0x0D5F })) do
    local ticks = nba.rom_read16(a + W)
    local info = inspect(nba.rom_read32(a + 5 * W))
    -- countdowns of at least 20 seconds (53 ticks a second); shorter ones are other screens
    if info and ticks >= 20 * 53 then
      callers[#callers + 1] = { addr = a, ticks = ticks, info = info }
      if ticks > max_ticks then max_ticks = ticks end
    end
  end
  if #callers == 0 then
    nba.log("select screen countdown not found in this game's ROM; nothing changed")
    return
  end
  local n = 0
  for _, c in ipairs(callers) do
    -- the longest countdown is team select; the others are initials entry
    if c.ticks == max_ticks or remove_initials then
      nba.rom_write32(c.addr + 5 * W, c.info.sucide)
      n = n + 1
    end
  end
  nba.log(string.format("countdown removed (%d of %d screens)", n, #callers))
end
