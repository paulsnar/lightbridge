# Lightbridge

![Aperture Science Hard Light Bridge](https://i.imgur.com/1E26beW.jpg)

A simple program to program your LPD8806-based LED strand.

The very basic usage:

```lua
set_rgb(
  1,    -- LED index, 1-based
  64,   -- red value 0-127
  32,   -- green value
  16    -- blue value
)
```

Rinse and repeat for any algorithm you can think of.

## Usage

So far this has only been tested on a couple of Raspberry Pis running Arch
Linux, but there shouldn't be much difference.

A couple of dependencies:

* Scripting environment: Lua (5.3 or higher)
* [`libbcm2835`](http://www.airspayce.com/mikem/bcm2835/) -- available in Arch
    Linux ARM `alarm` Pacman repo
* A working C compiler

Building: run `make`. You'll get a `lb2` executable, with which you can run a
Lua script.

## API

This is really barebones, but here we go:

### `config`

This global table contains these properties:

* `strand_length` specifies how many LEDs your strand has. The default is 160
  and can be adjusted at runtime.
* `frame_timeout` specifies for how long should the program sleep after
  flushing out an animation frame. This is specified in seconds, so you'll
  probably want to set it to a fraction.
* `latch_bytes` reflects how long is the latch prefix/suffix. Its value should
  not be smaller than `strand_length / 5`.
* `clock_divider` controls the frequency the SPI clock runs at. The values and
  speeds should match the ones found [here](https://www.raspberrypi.org/documentation/hardware/raspberrypi/spi/README.md) (section "Speed".)

### Functions

Note that all functions operate on a memory buffer, so their actions won't be
written out to the strand immediately. To do that you either call `flush()`
manually or wait until your program finishes and `flush()` will be called for
you.

* `set_rgb(led, r, g, b)` sets the `led`-th diode to the color specified by
  `r`, `g` and `b`, which should be integers with values between 0 and 127.
  The `led` index is 1-based.
* `fill(from, to, r, g, b)` or sets all LEDs between `from` and `to` (both
  inclusive) to the color specified by `r`, `g` and `b`. The same semantics
  apply as to `set_rgb`.
* `flush()` sends the current state of the LED buffer to the strand to be
  displayed. Unless it is called in the form of `flush(false)`, it will suspend
  the execution of the program for the time amount specified in
  `config.frame_timeout`.
* `sleep(amount)` suspends the execution of the program for `amount` seconds.
  It can also be specified as a fraction.

The standard Lua library is at your disposal, so feel free to go nuts, if you
don't mind losing some performance. (Hint: your script should mostly work to
just do math things, but nothing's stopping you to, i.e., react to system to
system events instead.)
