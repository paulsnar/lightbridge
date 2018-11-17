/*
 * Lightbridge
 *
 * © 2017-2018 paulsnar <paulsnar@paulsnar.lv>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <bcm2835.h>

#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NANO_INVERSE 1000000000
#define MAX_BRIGHTNESS 0x7F

#define BYTES_RGB(n) (3 * (n))
#define BYTES_TOTAL(n, l) (2 * l + BYTES_RGB(n))
#define CLAMP(n, min, max) \
  if (n < min) { n = min; } \
  else if (n > max) { n = max; }

static size_t strand_length = 0;
static unsigned long latch_bytes = 0, clock_divider = 0;
static struct timespec frame_l = { 0, 0 };

static void recreate_strand(lua_State *L)
{
  char *buf = lua_newuserdata(L,
    BYTES_TOTAL(strand_length, latch_bytes));
  lua_setglobal(L, "strand");

  memset(buf, 0, latch_bytes);
  memset(buf + latch_bytes, 0x80, BYTES_RGB(strand_length));
  memset(buf + latch_bytes + BYTES_RGB(strand_length), 0, latch_bytes);
}

static int _set_config_frame_timeout(lua_State *L)
{
  lua_Number s_fr = lua_tonumber(L, -1);

  frame_l.tv_sec = (int) floor(s_fr);
  frame_l.tv_nsec = (int) floor(fmodl(s_fr, 1) * NANO_INVERSE);

  return 0;
}

static int _set_config_strand_length(lua_State *L)
{
  lua_Integer len = lua_tointeger(L, -1);
  if (len == strand_length) {
    return 0;
  }

  strand_length = len;
  recreate_strand(L);
  return 0;
}

static int _set_config_latch_bytes(lua_State *L)
{
  lua_Integer bytes = lua_tointeger(L, -1);
  if (bytes == latch_bytes) {
    return 0;
  }

  latch_bytes = bytes;
  recreate_strand(L);
  return 0;
}

static int _set_config_clock_divider(lua_State *L)
{
  lua_Integer divider = lua_tointeger(L, -1);
  if (divider == clock_divider) {
    return 0;
  }

  bcm2835_spi_setClockDivider(divider);
  clock_divider = divider;
  return 0;
}

static int _config_get(lua_State *L)
{
  const char *key = lua_tostring(L, 2);

  if (strcmp("frame_timeout", key) == 0) {
    lua_pushnumber(L, frame_l.tv_sec + (frame_l.tv_nsec / NANO_INVERSE));
  } else if (strcmp("strand_length", key) == 0) {
    lua_pushinteger(L, strand_length);
  } else if (strcmp("latch_bytes", key) == 0) {
    lua_pushinteger(L, latch_bytes);
  } else if (strcmp("clock_divider", key) == 0) {
    lua_pushinteger(L, clock_divider);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int _config_set(lua_State *L)
{
  const char *key = lua_tostring(L, 2);

  if (strcmp("frame_timeout", key) == 0) {
    return _set_config_frame_timeout(L);
  } else if (strcmp("strand_length", key) == 0) {
    return _set_config_strand_length(L);
  } else if (strcmp("latch_bytes", key) == 0) {
    return _set_config_latch_bytes(L);
  } else if (strcmp("clock_divider", key) == 0) {
    return _set_config_clock_divider(L);
  }

  lua_pushfstring(L, "unknown config key: %s", key);
  lua_error(L);
  return -1;
}

static int _set_rgb(lua_State *L)
{
  if (lua_gettop(L) < 4) {
    lua_pushliteral(L, "set_rgb: not enough arguments");
    lua_error(L);
    return -1;
  }

  lua_getglobal(L, "strand");
  char *buf = (char*) lua_touserdata(L, -1);
  lua_pop(L, 1);

  int led_num = lua_tointeger(L, 1);
  if (led_num < 1 || led_num > strand_length) {
    lua_pushliteral(L, "set_rgb: invalid LED number specified");
    lua_error(L);
    return -1;
  }

  size_t base_offset = latch_bytes + BYTES_RGB(led_num - 1);

  lua_Integer
    r = lua_tointeger(L, 2),
    g = lua_tointeger(L, 3),
    b = lua_tointeger(L, 4);

  CLAMP(r, 0, MAX_BRIGHTNESS);
  CLAMP(g, 0, MAX_BRIGHTNESS);
  CLAMP(b, 0, MAX_BRIGHTNESS);

  buf[base_offset++] = 0x80 | b;
  buf[base_offset++] = 0x80 | r;
  buf[base_offset++] = 0x80 | g;

  return 0;
}

static int _fill(lua_State *L)
{
  if (lua_gettop(L) < 5) {
    lua_pushliteral(L, "fill: not enough arguments");
    lua_error(L);
    return -1;
  }

  lua_getglobal(L, "strand");
  char *buf = (char*) lua_touserdata(L, -1);
  lua_pop(L, 1);

  lua_Integer led_from = lua_tointeger(L, 1),
              led_to = lua_tointeger(L, 2);
  if (led_from < 1 || led_to < 1 ||
      led_from > strand_length || led_to > strand_length ||
      led_to < led_from) {
    lua_pushliteral(L, "fill: invalid LED range specified");
    lua_error(L);
    return -1;
  }

  size_t base_offset = latch_bytes + BYTES_RGB(led_from - 1);

  lua_Integer
    r = lua_tointeger(L, 3),
    g = lua_tointeger(L, 4),
    b = lua_tointeger(L, 5);

  CLAMP(r, 0, MAX_BRIGHTNESS);
  CLAMP(g, 0, MAX_BRIGHTNESS);
  CLAMP(b, 0, MAX_BRIGHTNESS);

  if (r == g && g == b) {
    // can shell out to memset
    memset(buf + base_offset, 0x80 | r, (led_to - led_from + 1) * 3);
  } else {
    for (int i = led_from; i <= led_to; i += 1) {
      buf[base_offset++] = 0x80 | b;
      buf[base_offset++] = 0x80 | r;
      buf[base_offset++] = 0x80 | g;
    }
  }

  return 0;
}

static int _flush(lua_State *L)
{
  lua_getglobal(L, "strand");
  char *buf = (char*) lua_touserdata(L, -1);
  lua_pop(L, 1);

  bcm2835_spi_writenb(buf, BYTES_TOTAL(strand_length, latch_bytes));

  if (!(lua_isboolean(L, 1) && lua_toboolean(L, 1) == 0)) {
    nanosleep(&frame_l, NULL);
  }

  return 0;
}

static int _sleep(lua_State *L)
{
  lua_Number sec_frac = lua_tonumber(L, 1);

  struct timespec sleep = {
    .tv_sec = (int) floor(sec_frac),
    .tv_nsec = (int) floor(fmodl(sec_frac, 1) * NANO_INVERSE)
  };
  nanosleep(&sleep, NULL);

  return 0;
}

jmp_buf finished;

void finish(int signal)
{
  longjmp(finished, 42);
}

int main(int argc, char *argv[])
{
  if (argc == 0) {
    fputs("Invalid invocation\n", stderr);
    return 1;
  } else if (argc == 1) {
    fprintf(stderr, "Usage: %s script.lua\n", argv[0]);
    return 1;
  }

  lua_State *L = NULL;
  char bcm_initialized = 0;

  L = luaL_newstate();
  if (L == NULL) {
    fputs("could not initialize Lua\n", stderr);
    goto exit;
  }
  luaL_openlibs(L);

  if ( ! bcm2835_init()) {
    errno = 1;
    fputs("could not initialize libbcm2835 (are you running as root?)\n",
      stderr);
    goto exit;
  }
  bcm_initialized |= 1;

  if ( ! bcm2835_spi_begin()) {
    errno = 1;
    fputs("could not initialize libbcm2835 SPI (are you running as root?)\n",
      stderr);
    goto exit;
  }
  bcm_initialized |= 2;

  lua_pushcfunction(L, _set_rgb);
  lua_setglobal(L, "set_rgb");
  lua_pushcfunction(L, _fill);
  lua_setglobal(L, "fill");
  lua_pushcfunction(L, _flush);
  lua_setglobal(L, "flush");
  lua_pushcfunction(L, _sleep);
  lua_setglobal(L, "sleep");

  lua_newtable(L); // config table
  lua_newtable(L); // its metatable
  lua_pushcfunction(L, _config_get);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, _config_set);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_pushvalue(L, -1);
  lua_setglobal(L, "config");

  lua_pushinteger(L, 160);
  lua_setfield(L, -2, "strand_length");
  lua_pushinteger(L, 5);
  lua_setfield(L, -2, "latch_bytes");
  lua_pushinteger(L, 256);
  lua_setfield(L, -2, "clock_divider");
  lua_pop(L, 1);

  char* source = argv[1];
  if (strcmp(source, "-") == 0) {
    source = "/dev/stdin";
  }

  signal(SIGINT, finish);
  signal(SIGTERM, finish);

  if (setjmp(finished) == 42) {
    lua_pushcfunction(L, _fill);
    lua_pushinteger(L, 1);
    lua_pushinteger(L, strand_length);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    lua_call(L, 5, 0);
    lua_pushcfunction(L, _flush);
    lua_call(L, 0, 0);
    goto exit;
  }

  if (luaL_dofile(L, source) != 0) {
    fprintf(stderr, "script error: %s\n", lua_tostring(L, -1));
    goto exit;
  }

  lua_getglobal(L, "flush");
  lua_call(L, 0, 0);

exit:
  if (bcm_initialized & 2) bcm2835_spi_end();
  if (bcm_initialized & 1) bcm2835_close();
  if (L != NULL) lua_close(L);

  if (errno) return 1;
  return 0;
}

// vim: set ts=2 sts=2 et sw=2 si: //
