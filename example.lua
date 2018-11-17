config.frame_timeout = 1/30
math.tau = math.pi * 2

local LEDS = config.strand_length
local LEDS_3 = math.floor(LEDS / 3)
local LIGHTNESS = 0x7F

local values = { }
for i = 1, LEDS do
  local v = math.sin(math.tau * i / LEDS)
  values[i] = math.floor(v * LIGHTNESS)
end

local o = 0
while true do
  for i = 1, LEDS do
    local rO = (o + i) % LEDS
    local gO = (rO + LEDS_3) % LEDS
    local bO = (gO + LEDS_3) % LEDS

    set_rgb(i, values[rO + 1], values[gO + 1], values[bO + 1])
  end

  flush()
  o = (o + 1) % LEDS
end
