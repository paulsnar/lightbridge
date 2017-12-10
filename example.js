config.frame_timeout = 1/30
Math.TAU = Math.PI * 2

var LEDS = 160,
    LEDS_3 = 53,
    LIGHTNESS = 1.0,
    frames = new Array(LEDS)

for (var i = 0; i < LEDS; i++) {
    var rO = i / LEDS,
        gO = ((i + LEDS_3) % LEDS) / LEDS,
        bO = ((i + LEDS_3 + LEDS_3) % LEDS) / LEDS

    var r = Math.sin(Math.TAU * rO) * LIGHTNESS,
        g = Math.sin(Math.TAU * gO) * LIGHTNESS,
        b = Math.sin(Math.TAU * bO) * LIGHTNESS

    frames[i] = [
        r > 0 ? r : 0,
        g > 0 ? g : 0,
        b > 0 ? b : 0,
    ]
}

var o = 0
while (true) {
    for (var i = 0; i < LEDS; i++) {
        var local_o = (o + i) % LEDS

        set_rgb(i, frames[local_o])
    }
    flush()
    o = (o + 1) % LEDS
}