/*
 * Lightbridge
 *
 * © 2017 paulsnar <paulsnar@paulsnar.lv>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <bcm2835.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

lua_Integer strand_length = DEFAULT_STRAND_LENGTH;

static inline char num_value(lua_State *L, int idx, char min, char max) {
    int val = lua_isinteger(L, idx) ? lua_tointeger(L, idx) :
        (int) floor(max * lua_tonumber(L, idx));
    if (val > max) return max;
    else if (val < min) return min;
    return val;
}

static int m_set_rgb(lua_State *L) {
    lua_getglobal(L, "strand");
    char *buf = (char*) lua_touserdata(L, -1);
    lua_pop(L, 1);

    char valid_table = lua_gettop(L) == 2 && lua_istable(L, 2);
    char valid_quad = lua_gettop(L) == 4;

    if (!(valid_table || valid_quad)) {
        lua_pushliteral(L, "not enough arguments");
        lua_error(L);
        return -1;
    }

    lua_Number led_num = luaL_optinteger(L, 1, 0);
    if (led_num <= 0 || led_num > strand_length) {
        lua_pushliteral(L, "invalid LED number specified");
        lua_error(L);
        return -1;
    }

    if (lua_gettop(L) == 2 && lua_istable(L, 2)) {
        lua_geti(L, 2, 1);
        lua_geti(L, 2, 2);
        lua_geti(L, 2, 3);
        lua_remove(L, 2);
    }

    if (lua_gettop(L) < 4) {
        lua_pushliteral(L, "not enough arguments");
        lua_error(L);
        return -1;
    }

    char
        r = num_value(L, 2, 0, MAX_LED_VAL),
        g = num_value(L, 3, 0, MAX_LED_VAL),
        b = num_value(L, 4, 0, MAX_LED_VAL);

    size_t base_offset = BYTES_LATCH_PRE + BYTES_RGB(led_num - 1);
    buf[base_offset++] = 0x80 | b;
    buf[base_offset++] = 0x80 | r;
    buf[base_offset++] = 0x80 | g;

    return 0;
}

static int m_all_off(lua_State *L) {
    lua_getglobal(L, "strand");
    char *buf = (char*) lua_touserdata(L, -1);
    lua_pop(L, -1);

    memset(buf + BYTES_LATCH_PRE, 0x80, BYTES_RGB(strand_length));

    return 0;
}

struct timespec sleep_spec = {
    .tv_sec = DEFAULT_SLEEP_SEC,
    .tv_nsec = DEFAULT_SLEEP_NSEC,
};

static int m_flush(lua_State *L) {
    lua_getglobal(L, "strand");
    char *buf = (char*) lua_touserdata(L, -1);
    lua_pop(L, 1);

    bcm2835_spi_writenb(buf, BYTES_TOTAL(strand_length));

    if (!(lua_isboolean(L, 1) && lua_toboolean(L, 1) == 0)) {
        nanosleep(&sleep_spec, NULL);
    }

    return 0;
}

static int m_sleep(lua_State *L) {
    lua_Number sec_frac = lua_tonumber(L, 1);

    struct timespec sleep = {
        .tv_sec = (int) floor(sec_frac),
        .tv_nsec = (int) floor(fmodl(sec_frac, 1) * SEC_TO_NSEC)
    };
    nanosleep(&sleep, NULL);

    return 0;
}

static int m_fill(lua_State *L) {
    lua_getglobal(L, "strand");
    char *buf = (char*) lua_touserdata(L, -1);
    lua_pop(L, 1);

    lua_Integer
        led_from = lua_tointeger(L, 1),
        led_to = lua_tointeger(L, 2);

    if (led_from < 1 || led_to < 1 ||
        led_from > strand_length || led_to > strand_length) {
        lua_pushliteral(L, "invalid LED specified");
        lua_error(L);
        return -1;
    }

    if (lua_gettop(L) == 3) {
        if (lua_isinteger(L, 3)) {
            int val = num_value(L, 3, 0, MAX_LED_VAL);
            memset(
                buf + BYTES_LATCH_PRE + BYTES_RGB(led_from - 1),
                (char) val | 0x80,
                BYTES_RGB(led_to - led_from - 1));
            return 0;
        } else if (lua_istable(L, 3)) {
            lua_geti(L, 3, 1);
            lua_geti(L, 3, 2);
            lua_geti(L, 3, 3);
            lua_remove(L, 3);
        }
    }

    int
        r = num_value(L, 3, 0, MAX_LED_VAL),
        g = num_value(L, 4, 0, MAX_LED_VAL),
        b = num_value(L, 5, 0, MAX_LED_VAL);

    for (int i = BYTES_RGB(led_from - 1); i < BYTES_RGB(led_to); ) {
        buf[BYTES_LATCH_PRE + i++] = 0x80 | b;
        buf[BYTES_LATCH_PRE + i++] = 0x80 | r;
        buf[BYTES_LATCH_PRE + i++] = 0x80 | g;
    }

    return 0;
}

lua_Number last_frame_timeout_value =
    DEFAULT_SLEEP_SEC + (DEFAULT_SLEEP_NSEC / SEC_TO_NSEC);

static int _m_config_get(lua_State *L) {
    lua_pushliteral(L, "frame_timeout");
    if (lua_compare(L, 2, -1, LUA_OPEQ)) {
        lua_pop(L, 1);
        lua_pushnumber(L, last_frame_timeout_value);
        return 1;
    }
    lua_pop(L, 1);

    lua_pushliteral(L, "strand_length");
    if (lua_compare(L, 2, -1, LUA_OPEQ)) {
        lua_pop(L, 1);
        lua_pushinteger(L, strand_length);
        return 1;
    }
    lua_pop(L, 1);

    lua_pushnil(L);
    return 1;

}

static int _m_config_set(lua_State *L) {
    lua_pushliteral(L, "frame_timeout");
    if (lua_compare(L, 2, -1, LUA_OPEQ)) {
        lua_pop(L, 1);
        if ( ! lua_isnumber(L, 3)) {
            lua_pushliteral(L, "invalid assignment type - expected number");
            lua_error(L);
            return -1;
        }

        last_frame_timeout_value = lua_tonumber(L, 3);

        sleep_spec.tv_sec = (int) floor(last_frame_timeout_value);
        sleep_spec.tv_nsec =
            (int) floor(fmodl(last_frame_timeout_value, 1) * SEC_TO_NSEC);

        return 0;
    }
    lua_pop(L, 1);

    lua_pushliteral(L, "strand_length");
    if (lua_compare(L, 2, -1, LUA_OPEQ)) {
        lua_pop(L, 1);

        if ( ! lua_isinteger(L, 3)) {
            lua_pushliteral(L, "invalid assignment type - expected integer");
            lua_error(L);
            return -1;
        }

        lua_Integer proposed_strand_length = lua_tointeger(L, 3);
        if (proposed_strand_length == strand_length) {
            return 0;
        }

        char *buf = lua_newuserdata(L, BYTES_TOTAL(proposed_strand_length));
        strand_length = proposed_strand_length;

        memset(buf, 0, BYTES_LATCH_PRE);
        memset(buf + BYTES_LATCH_PRE, 0x80, BYTES_RGB(strand_length));
        memset(buf + BYTES_LATCH_PRE + BYTES_RGB(strand_length), 0, BYTES_LATCH_POST);

        lua_setglobal(L, "strand");
        return 0;
    }
    lua_pop(L, 1);

    lua_pushfstring(L,
        "tried to set invalid config key %s", lua_tostring(L, 2));
    lua_error(L);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc == 0) {
        puts("Invalid invocation");
        return 1;
    } else if (argc == 1) {
        printf("Usage: %s script.lua\n", argv[0]);
        return 0;
    }

    lua_State *L = NULL;
    char bcm_initialized = 0;

    L = luaL_newstate();
    if (L == NULL) {
        puts("Error: Could not initialize Lua");
        return 1;
    }
    luaL_openlibs(L);

    if ( ! bcm2835_init()) {
        errno = 1;
        puts("Error: Could not initialize BCM2835 (are you running as root?)");
        goto exit;
    }
    bcm_initialized |= 1;

    if ( ! bcm2835_spi_begin()) {
        errno = 1;
        puts("Error: Could not initialize SPI (are you running as root?)");
        goto exit;
    }
    bcm_initialized |= 2;

    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512);

    char *buf = (char*) lua_newuserdata(L, BYTES_TOTAL(strand_length));
    if (buf == NULL) {
        errno = 1;
        puts("Error: Out of memory");
        goto exit;
    }
    lua_setglobal(L, "strand");

    memset(buf, 0, BYTES_LATCH_PRE);
    memset(buf + BYTES_LATCH_PRE, 0x80, BYTES_RGB(strand_length));
    memset(buf + BYTES_LATCH_PRE + BYTES_RGB(strand_length), 0, BYTES_LATCH_POST);

    lua_pushcfunction(L, m_set_rgb);
    lua_setglobal(L, "set_rgb");

    lua_pushcfunction(L, m_all_off);
    lua_setglobal(L, "all_off");

    lua_pushcfunction(L, m_fill);
    lua_setglobal(L, "fill");

    lua_pushcfunction(L, m_flush);
    lua_setglobal(L, "flush");

    lua_pushcfunction(L, m_sleep);
    lua_setglobal(L, "sleep");

    lua_newtable(L); // table itself
    lua_newtable(L); // metatable
    lua_pushcfunction(L, _m_config_get);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, _m_config_set);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "config");

    if (luaL_dofile(L, argv[1]) != 0) {
        printf("Error: could not run file:\n  %s\n", lua_tostring(L, -1));
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
