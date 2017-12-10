config.frame_timeout = 1/30
math.tau = math.pi * 2

local LEDS = config.strand_length
local LEDS_3 = math.floor(LEDS / 3)
local LIGHTNESS = 1.0

local frames = { }
for i = 1, LEDS do
  local rO, gO, bO =
    i,
    (i + LEDS_3) % LEDS,
    (i - LEDS_3) % LEDS

  local r, g, b =
    math.sin(math.tau * (rO / LEDS)) * LIGHTNESS,
    math.sin(math.tau * (gO / LEDS)) * LIGHTNESS,
    math.sin(math.tau * (bO / LEDS)) * LIGHTNESS

  if r < 0 then r = 0 end
  if g < 0 then g = 0 end
  if b < 0 then b = 0 end

  frames[i] = { r, g, b }
end

local o = 0
while true do
  for i = 1, LEDS do
    local o = (o + i) % LEDS
    o = o + 1
    set_rgb(i, frames[o][1], frames[o][2], frames[o][3])
  end

  flush()
  o = (o + 1) % LEDS
end
