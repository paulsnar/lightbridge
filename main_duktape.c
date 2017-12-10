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

#include <duktape.h>

#include <bcm2835.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

unsigned long strand_length = 0;

static inline char num_value(duk_double_t num, char min, char max) {
    int val = num > 1 ? (int) floor(num) : round(MAX_LED_VAL * num);
    if (val > max) return max;
    else if (val < min) return min;
    return (char) val;
}

static duk_ret_t m_set_rgb(duk_context *ctx) {
    duk_get_global_string(ctx, "strand");
    char *buf = (char*) duk_get_buffer_data(ctx, -1, NULL);
    duk_pop(ctx);

    int led_num = duk_require_int(ctx, 0);
    if (led_num >= strand_length || led_num < 0) {
        return DUK_RET_RANGE_ERROR;
    }

    if (duk_get_top(ctx) == 2 && duk_is_array(ctx, 1)) {
        duk_get_prop_index(ctx, 1, 0);
        duk_get_prop_index(ctx, 1, 1);
        duk_get_prop_index(ctx, 1, 2);
        duk_remove(ctx, 1);
    }

    if (duk_get_top(ctx) < 4) {
        return DUK_RET_TYPE_ERROR;
    }

    int
        r = num_value(duk_require_number(ctx, 1), 0, MAX_LED_VAL),
        g = num_value(duk_require_number(ctx, 2), 0, MAX_LED_VAL),
        b = num_value(duk_require_number(ctx, 3), 0, MAX_LED_VAL);

    size_t base_offset = BYTES_RGB(led_num);
    buf[base_offset++] = 0x80 | b;
    buf[base_offset++] = 0x80 | r;
    buf[base_offset++] = 0x80 | g;

    return 0;
}

static duk_ret_t m_all_off(duk_context *ctx) {
    duk_get_global_string(ctx, "strand");
    char *buf = (char*) duk_get_buffer_data(ctx, -1, NULL);
    duk_pop(ctx);

    memset(buf, 0x80, BYTES_RGB(strand_length));

    return 0;
}

struct timespec sleep_spec = {
    .tv_sec = DEFAULT_SLEEP_SEC,
    .tv_nsec = DEFAULT_SLEEP_NSEC,
};

static duk_ret_t m_flush(duk_context *ctx) {
    duk_get_global_string(ctx, "strand");
    duk_get_prop_string(ctx, -1, "raw");
    char *buf = (char*) duk_get_buffer_data(ctx, -1, NULL);
    duk_pop_2(ctx);

    bcm2835_spi_writenb(buf, BYTES_TOTAL(strand_length));

    if (!(duk_is_boolean(ctx, 0) && duk_get_boolean(ctx, 0) == 0)) {
        nanosleep(&sleep_spec, NULL);
    }

    return 0;
}

duk_double_t last_frame_timeout_value =
    DEFAULT_SLEEP_SEC + (DEFAULT_SLEEP_NSEC / SEC_TO_NSEC);

static duk_ret_t _m_get_config_frame_timeout(duk_context *ctx) {
    duk_push_number(ctx, last_frame_timeout_value);
    return 1;
}

static duk_ret_t _m_set_config_frame_timeout(duk_context *ctx) {
    last_frame_timeout_value = duk_require_number(ctx, 0);

    sleep_spec.tv_sec = (int) floor(last_frame_timeout_value);
    sleep_spec.tv_nsec =
        (int) floor(fmodl(last_frame_timeout_value, 1) * SEC_TO_NSEC);

    return 0;
}

static duk_ret_t m_sleep(duk_context *ctx) {
    duk_double_t sec_frac = duk_require_number(ctx, 0);

    struct timespec sleep = {
        .tv_sec = (int) floor(sec_frac),
        .tv_nsec = (int) floor(fmodl(sec_frac, 1) * SEC_TO_NSEC)
    };
    nanosleep(&sleep, NULL);

    return 0;
}

static duk_ret_t _m_get_config_strand_length(duk_context *ctx) {
    duk_push_uint(ctx, (duk_uint_t) strand_length);
    return 1;
}

static duk_ret_t _m_set_config_strand_length(duk_context *ctx) {
    duk_uint_t proposed_strand_length = duk_require_uint(ctx, 0);
    if (proposed_strand_length == strand_length) {
        return 0;
    }

    char *buf = duk_push_fixed_buffer(ctx, BYTES_TOTAL(proposed_strand_length));
    strand_length = proposed_strand_length;

    duk_push_buffer_object(ctx, -1, BYTES_LATCH_PRE, BYTES_RGB(strand_length),
        DUK_BUFOBJ_UINT8ARRAY);
    duk_push_buffer_object(ctx, -2, 0, BYTES_TOTAL(strand_length),
        DUK_BUFOBJ_ARRAYBUFFER);
    duk_put_prop_string(ctx, -2, "raw");
    duk_put_global_string(ctx, "strand");

    memset(buf, 0, BYTES_LATCH_PRE);
    memset(buf + BYTES_LATCH_PRE, 0x80, BYTES_RGB(strand_length));
    memset(buf + BYTES_LATCH_PRE + BYTES_RGB(strand_length), 0, BYTES_LATCH_POST);

    return 0;
}

static duk_ret_t m_fill(duk_context *ctx) {
    duk_get_global_string(ctx, "strand");
    char *buf = (char*) duk_get_buffer_data(ctx, -1, NULL);
    duk_pop(ctx);

    int
        led_from = (int) floor(duk_require_number(ctx, 0)),
        led_to = (int) floor(duk_require_number(ctx, 1));

    if (led_from < 0 || led_to < 0 ||
        led_from >= strand_length || led_to >= strand_length) {
        return DUK_RET_RANGE_ERROR;
    }

    int val;

    if (duk_get_top(ctx) == 3) {
        if (duk_is_number(ctx, 2)) {
            // brightness fill -- shell out to memset
            val = num_value(duk_require_number(ctx, 2), 0, MAX_LED_VAL);
            memset(
                buf + BYTES_LATCH_PRE + BYTES_RGB(led_from),
                (char) val | 0x80,
                BYTES_RGB(led_to - led_from + 1));
            return 0;
        } else if (duk_is_array(ctx, 2)) {
            duk_get_prop_index(ctx, 2, 0);
            duk_get_prop_index(ctx, 2, 1);
            duk_get_prop_index(ctx, 2, 2);
            duk_remove(ctx, 3);
        }
    }

    int
        r = num_value(duk_require_number(ctx, 2), 0, MAX_LED_VAL),
        g = num_value(duk_require_number(ctx, 3), 0, MAX_LED_VAL),
        b = num_value(duk_require_number(ctx, 4), 0, MAX_LED_VAL);

    for (int i = BYTES_RGB(led_from); i <= BYTES_RGB(led_to); ) {
        buf[i++] = 0x80 | b;
        buf[i++] = 0x80 | r;
        buf[i++] = 0x80 | g;
    }

    return 0;
}

static duk_ret_t m_print(duk_context *ctx) {
    duk_size_t str_len;
    const char *str = duk_safe_to_lstring(ctx, 0, &str_len);
    printf("%.*s\n", str_len, str);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 0) {
        puts("Invalid invocation");
        return 1;
    } else if (argc == 1) {
        printf("Usage: %s script.js\n", argv[0]);
        return 0;
    }

    duk_context *ctx = NULL;
    char bcm_initialized = 0;
    FILE *source = NULL;
    char *sourcebuf = NULL;

    ctx = duk_create_heap_default();
    if (ctx == NULL) {
        puts("Error: could not initialize Duktape");
        return 1;
    }

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

    duk_push_c_function(ctx, m_set_rgb, DUK_VARARGS);
    duk_put_global_string(ctx, "set_rgb");

    duk_push_c_function(ctx, m_all_off, 0);
    duk_put_global_string(ctx, "all_off");

    duk_push_c_function(ctx, m_flush, 1);
    duk_put_global_string(ctx, "flush");

    duk_push_c_function(ctx, m_fill, DUK_VARARGS);
    duk_put_global_string(ctx, "fill");

    duk_push_c_function(ctx, m_sleep, 1);
    duk_put_global_string(ctx, "sleep");

    duk_push_c_function(ctx, m_print, 1);
    duk_put_global_string(ctx, "print");


    duk_idx_t idx_obj_config = duk_push_object(ctx);

    duk_push_string(ctx, "frame_timeout");
    duk_push_c_function(ctx, _m_get_config_frame_timeout, 0);
    duk_push_c_function(ctx, _m_set_config_frame_timeout, 1);
    duk_def_prop(ctx, idx_obj_config,
        DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

    duk_push_string(ctx, "strand_length");
    duk_push_c_function(ctx, _m_get_config_strand_length, 0);
    duk_push_c_function(ctx, _m_set_config_strand_length, 1);
    duk_def_prop(ctx, idx_obj_config,
        DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

    duk_put_global_string(ctx, "config");

    duk_push_c_function(ctx, _m_set_config_strand_length, 1);
    duk_push_uint(ctx, DEFAULT_STRAND_LENGTH);
    duk_call(ctx, 1);
    duk_pop(ctx);

    source = fopen(argv[1], "r");
    if (source == NULL) {
        printf("Error: Could not open source file: %s\n",
            strerror(errno));
        goto exit;
    }

    fseek(source, 0, SEEK_END);
    long fsize = ftell(source);
    rewind(source);

    sourcebuf = malloc(fsize);
    if (sourcebuf == NULL) {
        printf("Error: Could not allocate memory for reading source: %s\n",
            strerror(errno));
        goto exit;
    }

    if (fread(sourcebuf, 1, fsize, source) < fsize) {
        if (ferror(source) != 0) {
            errno = 1;
            puts("Error: Could not read source file");
            goto exit;
        }
    }

    fclose(source);
    source = NULL;

    duk_push_lstring(ctx, sourcebuf, fsize);
    free(sourcebuf);
    sourcebuf = NULL;

    duk_push_string(ctx, argv[1]);

    duk_compile(ctx, 0);
    if (duk_pcall(ctx, 0) == DUK_EXEC_ERROR) {
        errno = 1;
        const char *errmsg = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "Error in user code: \n  %s\n", errmsg);
        goto exit;
    }

    duk_get_global_string(ctx, "flush");
    duk_call(ctx, 0);

exit:
    if (sourcebuf != NULL) free(sourcebuf);
    if (source != NULL) fclose(source);
    if (bcm_initialized & 2) bcm2835_spi_end();
    if (bcm_initialized & 1) bcm2835_close();
    if (ctx != NULL) duk_destroy_heap(ctx);

    if (errno) return 1;
    return 0;
}
