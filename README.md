# Lightbridge

![Aperture Science Hard Light Bridge](https://i.imgur.com/1E26beW.jpg)

A simple program to program your LPD8806-based LED strand.

Currently you can use either Lua or JS (via Duktape) for writing strand
programs. The basic principles are the same:

```lua
set_rgb(
  1,    -- LED index, 1-based
  64,   -- red value 0-127
  32,   -- green value
  16    -- blue value
)
```

```js
set_rgb(
  1,    // LED index, 0-based
  64,   // red value 0-127
  32,   // green value
  16    // blue value
)
```

An API documentation is coming soon.

## Usage

So far this has only been tested on a couple of Raspberry Pis running Arch
Linux, but there shouldn't be much difference.

A couple of dependencies:

* Scripting environment: Lua (5.3 or higher) and/or Duktape
* [`libbcm2835`](http://www.airspayce.com/mikem/bcm2835/) -- available in Arch
    Linux ARM `alarm` Pacman repo
* A working C compiler

Perform `make lightbridge-lua` or `make lightbridge-js` and you should be good
to go! Do `./lightbridge-lua program.lua` or `./lightbridge-js program.js`.

*Please note* that right now both of these programs require administrator
privileges. This goes against many security principles (usually), and I'd
prefer them not to, but currently I haven't looked into it enough.

## API

This is really barebones, but here we go:

### `config`

This global object or table contains two properties:

* `strand_length` specifies how many LEDs your strand has. The default is 160
  based on what strand I had at the time of development, but can be adjusted
  at runtime.
* `frame_timeout` specifies for how long should the program sleep after
  flushing out an animation frame. This is specified in seconds, so you'll
  probably want to set it to a fraction.

### Functions

Note that all functions operate on a memory buffer, so their actions won't be
written out to the strand immediately. To do that you either call `flush()`
manually or wait until your program finishes when it's called for you.

* `set_rgb(led, r, g, b)` or `set_rgb(led, [r, g, b])` sets the `led` specified
  to the color specified by `r`, `g` and `b`. All of them can be either
  integers between 0 and 127 (other values will be clipped to these ones) or
  floats.  
  *Implementation detail*: in Lua `led` is 1-based while in JS it's 0-based.
  This is to conform better with the language standards.  
  *Implementation detail*: since all JS numbers are technically floats, only
  values smaller than 1 will be treated as floats by the runtime. That means
  you cannot use 1.0 as a fractional color amount because that will be
  interpreted as absolute one (e.g. 1/127) instead.
* `fill(from, to, r, g, b)` or `fill(from, to, [r, g, b])` sets all LEDs
  between `from` and `to` (both inclusive) to the color specified by `r`, `g`
  and `b`. The same caveats apply as to `set_rgb`.
* `all_off()` sets all LEDs to be off.
* `flush()` sends the current state of the LED buffer to the strand to be
  displayed. Unless it is called in the form of `flush(false)`, it will suspend
  the execution of the program for the time amount specified in
  `config.frame_timeout`.
* `sleep(amount)` suspends the execution of the program for `amount` seconds.
  It can also be specified as a fraction.
* *JS only*: `print(str)` outputs `str` to stdout.
