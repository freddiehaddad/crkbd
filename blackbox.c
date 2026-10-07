// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "blackbox.h"

#ifdef OLED_ENABLE

#    include "atomic_util.h"
#    include "sync_timer.h"
#    include "transactions.h"

#    if OLED_DISPLAY_WIDTH != 128 || OLED_DISPLAY_HEIGHT != 32 || OLED_FONT_WIDTH != 6 || OLED_FONT_HEIGHT != 8
#        error "Blackbox requires 128x32 OLEDs with the standard 6x8 font."
#    endif

#    ifndef WPM_ENABLE
#        error "Blackbox requires WPM_ENABLE = yes."
#    endif

#    define BLACKBOX_IDLE_MS 60000UL
#    define BLACKBOX_SHUTDOWN_MS 700
#    define BLACKBOX_RGB_MS 2500
#    define BLACKBOX_SYNC_MS 50
#    define BLACKBOX_HEARTBEAT_MS 1000
#    define BLACKBOX_LINK_TIMEOUT_MS 3000
#    define BLACKBOX_SAMPLE_MS 500
#    define BLACKBOX_HISTORY_SIZE 44

enum {
    BB_GAMING     = 1 << 0,
    BB_KEY_HELD   = 1 << 1,
    BB_RGB_RECENT = 1 << 2,
    BB_RGB_ON     = 1 << 3,
};

typedef struct {
    uint8_t mode;
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;
    uint8_t speed;
    uint8_t flags;
} blackbox_rgb_t;

typedef struct {
    uint32_t       activity_at;
    uint32_t       rgb_at;
    layer_state_t  layers;
    uint8_t        mods;
    uint8_t        leds;
    uint8_t        wpm;
    uint8_t        flags;
    blackbox_rgb_t rgb;
} blackbox_state_t;

STATIC_ASSERT(sizeof(blackbox_state_t) <= RPC_M2S_BUFFER_SIZE, "Blackbox exceeds the split RPC buffer.");
STATIC_ASSERT(MATRIX_ROWS == 8 && MATRIX_COLS == 6, "Blackbox blueprints require the Corne matrix.");

typedef enum {
    BB_PAGE_NONE,
    BB_PAGE_STATUS,
    BB_PAGE_TELEMETRY,
    BB_PAGE_BLUEPRINT,
    BB_PAGE_RGB,
    BB_PAGE_SHUTDOWN,
    BB_PAGE_SLEEP,
    BB_PAGE_LINK,
} blackbox_page_t;

static blackbox_state_t state;
static blackbox_state_t last_sent;
static bool             gaming_enabled;
static bool             rgb_notified;
static uint32_t         rgb_at;
static bool             received_state;
static uint32_t         received_at;
static bool             link_ok;
static uint32_t         last_attempt;
static uint32_t         last_success;
static blackbox_page_t  current_page;
static uint8_t          history[BLACKBOX_HISTORY_SIZE];
static uint8_t          history_head;
static uint32_t         history_at;

static uint32_t elapsed_since(uint32_t now, uint32_t then) {
    // A split clock correction can briefly put a received timestamp in the future.
    return (int32_t)(now - then) < 0 ? 0 : now - then;
}

void blackbox_set_gaming_mode(bool enabled) {
    gaming_enabled = enabled;
}

void blackbox_note_key(uint16_t keycode) {
#    ifdef RGB_MATRIX_ENABLE
    if (IS_RGB_MATRIX_KEYCODE(keycode)) {
        rgb_at       = sync_timer_read32();
        rgb_notified = true;
    }
#    endif
}

static void receive_state(uint8_t in_length, const void *in_data, uint8_t out_length, void *out_data) {
    if (in_length != sizeof(state) || out_length != 0) {
        received_state = false;
        return;
    }

    memcpy(&state, in_data, sizeof(state));
    received_at    = timer_read32();
    received_state = true;
}

void keyboard_post_init_user(void) {
    transaction_register_rpc(BLACKBOX_SYNC, receive_state);
}

static void capture_state(void) {
    state.activity_at = last_input_activity_time();
    state.rgb_at      = rgb_at;
    state.layers      = layer_state | default_layer_state | (layer_state_t)1;
    state.mods        = get_mods() | get_weak_mods();
#    ifndef NO_ACTION_ONESHOT
    state.mods |= get_oneshot_mods() | get_oneshot_locked_mods();
#    endif
    state.leds  = host_keyboard_led_state().raw;
    state.wpm   = get_current_wpm();
    state.flags = gaming_enabled ? BB_GAMING : 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        if (matrix_get_row(row)) {
            state.flags |= BB_KEY_HELD;
            break;
        }
    }

#    ifdef RGB_MATRIX_ENABLE
    if (rgb_notified && elapsed_since(sync_timer_read32(), rgb_at) < BLACKBOX_RGB_MS) {
        state.flags |= BB_RGB_RECENT;
    }
    if (rgb_matrix_is_enabled()) {
        state.flags |= BB_RGB_ON;
    }
    state.rgb.mode       = rgb_matrix_get_mode();
    state.rgb.hue        = rgb_matrix_get_hue();
    state.rgb.saturation = rgb_matrix_get_sat();
    state.rgb.value      = rgb_matrix_get_val();
    state.rgb.speed      = rgb_matrix_get_speed();
    state.rgb.flags      = rgb_matrix_get_flags();
#    endif
}

void housekeeping_task_user(void) {
    if (!is_keyboard_master()) {
        return;
    }

    capture_state();
    if (timer_elapsed32(last_attempt) < BLACKBOX_SYNC_MS) {
        return;
    }
    if (link_ok && memcmp(&state, &last_sent, sizeof(state)) == 0 && timer_elapsed32(last_success) < BLACKBOX_HEARTBEAT_MS) {
        return;
    }

    last_attempt = timer_read32();
    bool sent    = transaction_rpc_send(BLACKBOX_SYNC, sizeof(state), &state);
    if (sent) {
        memcpy(&last_sent, &state, sizeof(state));
        last_success = timer_read32();
    } else if (link_ok) {
        dprint("Blackbox: split display sync failed\n");
    }
    link_ok = sent;
}

static void write_text(uint8_t col, uint8_t row, const char *text, bool invert) {
    oled_set_cursor(col, row);
    oled_write_P(text, invert);
}

static void write_number(uint8_t col, uint8_t row, uint8_t value) {
    oled_set_cursor(col, row);
    oled_write(get_u8_str(value, '0'), false);
}

static void render_status(const blackbox_state_t *view) {
    bool  gaming = view->flags & BB_GAMING;
    led_t leds   = {.raw = view->leds};

    write_text(0, 0, gaming ? PSTR("BLACKBOX // ARMED") : PSTR("BLACKBOX // READY"), gaming);
    write_text(17, 0, PSTR("    "), gaming);
    write_text(0, 1, PSTR("BASE"), false);
    write_text(6, 1, PSTR("CAP"), leds.caps_lock);
    write_text(11, 1, PSTR("NUM"), leds.num_lock);
    write_text(16, 1, PSTR("SCR"), leds.scroll_lock);
    write_text(0, 2, PSTR("CTRL"), view->mods & MOD_MASK_CTRL);
    write_text(6, 2, PSTR("ALT"), view->mods & MOD_MASK_ALT);
    write_text(11, 2, PSTR("SHFT"), view->mods & MOD_MASK_SHIFT);
    write_text(17, 2, PSTR("GUI"), view->mods & MOD_MASK_GUI);
    write_text(0, 3, gaming ? PSTR("META:SHIFT") : PSTR("META:GUI  "), false);
    write_text(12, 3, PSTR("LINK:"), false);
    write_text(18, 3, !is_keyboard_master() || link_ok ? PSTR("OK") : PSTR("--"), false);
}

static bool update_history(uint32_t now, uint8_t wpm) {
    uint32_t periods = elapsed_since(now, history_at) / BLACKBOX_SAMPLE_MS;
    if (!periods) {
        return false;
    }

    history_at += periods * BLACKBOX_SAMPLE_MS;
    uint8_t samples = periods < BLACKBOX_HISTORY_SIZE ? periods : BLACKBOX_HISTORY_SIZE;
    while (samples--) {
        history[history_head] = samples ? 0 : wpm;
        if (++history_head == BLACKBOX_HISTORY_SIZE) {
            history_head = 0;
        }
    }
    return true;
}

static void render_graph(void) {
    for (uint8_t i = 0; i < BLACKBOX_HISTORY_SIZE; ++i) {
        uint8_t  sample = history[(history_head + i) % BLACKBOX_HISTORY_SIZE];
        uint8_t  height = (uint16_t)(sample > 160 ? 160 : sample) * 15 / 160;
        uint16_t pixels = (uint16_t)(0xFFFFUL << (15 - height));
        uint8_t  x      = 40 + i * 2;
        oled_write_raw_byte((char)pixels, 256 + x);
        oled_write_raw_byte((char)(pixels >> 8), 384 + x);
        oled_write_raw_byte(0, 256 + x + 1);
        oled_write_raw_byte((char)0x80, 384 + x + 1);
    }
}

static void render_reactor(const blackbox_state_t *view, uint32_t now) {
    uint8_t level = view->wpm / 20;
    uint8_t phase = now / (view->wpm > 100 ? 100 : 200) % 6;

    for (uint8_t x = 0; x < 32; ++x) {
        uint32_t pixels = 0;
        if (x >= 3 && x <= 28) {
            uint8_t inset = x < 8 ? 8 - x : x > 23 ? x - 23 : 0;
            pixels        = (1UL << (1 + inset)) | (1UL << (22 - inset));
            if (x == 3 || x == 28) {
                pixels |= 0x0003FFC0UL;
            }
            if (x >= 10 && x <= 21) {
                for (uint8_t bar = 0; bar < 6; ++bar) {
                    if (bar < level || bar == phase) {
                        pixels |= 3UL << (20 - bar * 3);
                    }
                }
            }
        }
        if ((view->flags & BB_GAMING) && (x == 0 || x == 31)) {
            pixels = 0x00003C00UL;
        }
        for (uint8_t page = 0; page < 3; ++page) {
            oled_write_raw_byte((char)(pixels >> (page * 8)), (page + 1) * 128 + x);
        }
    }
}

static void render_telemetry(const blackbox_state_t *view, uint32_t now, bool redraw_graph) {
    bool gaming = view->flags & BB_GAMING;
    write_text(0, 0, PSTR("CORE  WPM:"), false);
    write_number(10, 0, view->wpm);
    write_text(15, 0, gaming ? PSTR("ARMED") : PSTR("READY"), gaming);
    write_text(7, 1, PSTR("22s / 160 WPM"), false);
    render_reactor(view, now);
    if (redraw_graph) {
        render_graph();
    }
}

typedef struct {
    uint16_t keycode;
    char     label[4];
} blackbox_legend_t;

static const blackbox_legend_t PROGMEM legends[] = {
    {KC_NO, "---"}, {KC_TAB, "TAB"}, {KC_ENT, "ENT"}, {KC_ESC, "ESC"}, {KC_BSPC, "BSP"}, {KC_DEL, "DEL"}, {KC_SPC, "SPC"}, {KC_LCTL, "CTL"}, {KC_LALT, "ALT"}, {KC_LSFT, "SFT"}, {KC_LGUI, "GUI"}, {KC_LEFT, "<-"}, {KC_DOWN, "DN"}, {KC_UP, "UP"}, {KC_RGHT, "->"}, {KC_HOME, "HOM"}, {KC_END, "END"}, {KC_PGUP, "PGU"}, {KC_PGDN, "PGD"}, {KC_PSCR, "PRT"}, {C(KC_SPC), "TMX"}, {S(KC_PSCR), "SNP"}, {MO(1), "SYM"}, {MO(2), "NAV"}, {MO(3), "SYS"}, {QK_BOOT, "RST"}, {RM_TOGG, "RGB"}, {RM_NEXT, "FX+"}, {RM_PREV, "FX-"}, {RM_HUEU, "H+"}, {RM_HUED, "H-"}, {RM_SATU, "S+"}, {RM_SATD, "S-"}, {RM_VALU, "V+"}, {RM_VALD, "V-"}, {RM_SPDU, "P+"}, {RM_SPDD, "P-"}, {RM_FLGN, "F+"}, {RM_FLGP, "F-"},
};

static void key_label(uint16_t keycode, bool gaming, char label[4]) {
    memset(label, ' ', 3);
    label[3] = '\0';
    if (gaming && keycode == KC_LGUI) {
        keycode = KC_LSFT;
    }
    for (uint8_t i = 0; i < ARRAY_SIZE(legends); ++i) {
        if (keycode == pgm_read_word(&legends[i].keycode)) {
            for (uint8_t j = 0; j < 3; ++j) {
                char c = pgm_read_byte(&legends[i].label[j]);
                if (!c) {
                    break;
                }
                label[j] = c;
            }
            return;
        }
    }

    bool shifted = false;
    if (IS_QK_MODS(keycode) && (QK_MODS_GET_MODS(keycode) & 0x0F) == MOD_LSFT) {
        shifted = true;
        keycode = QK_MODS_GET_BASIC_KEYCODE(keycode);
    }
    if (keycode >= KC_A && keycode <= KC_Z) {
        label[0] = 'A' + keycode - KC_A;
    } else if (keycode >= KC_1 && keycode <= KC_0) {
        static const char PROGMEM shifted_digits[] = "!@#$%^&*()";
        label[0]                                   = shifted ? pgm_read_byte(&shifted_digits[keycode - KC_1]) : '0' + (keycode - KC_1 + 1) % 10;
    } else if (keycode >= KC_MINS && keycode <= KC_SLSH) {
        static const char PROGMEM punctuation[2][13] = {"-=[]\\#;'`,./", "_+{}|~:\"~<>?"};
        label[0]                                     = pgm_read_byte(&punctuation[shifted][keycode - KC_MINS]);
    } else if (keycode >= KC_F1 && keycode <= KC_F12) {
        uint8_t number = keycode - KC_F1 + 1;
        label[0]       = 'F';
        label[1]       = '0' + (number < 10 ? number : number / 10);
        if (number >= 10) {
            label[2] = '0' + number % 10;
        }
    } else {
        label[0] = label[1] = label[2] = '?';
    }
}

static uint16_t resolved_keycode(layer_state_t layers, keypos_t key) {
    for (int8_t layer = get_highest_layer(layers); layer >= 0; --layer) {
        if (layers & ((layer_state_t)1 << layer)) {
            uint16_t keycode = keymap_key_to_keycode(layer, key);
            if (keycode != KC_TRNS) {
                return keycode;
            }
        }
    }
    return KC_NO;
}

static void render_key(const blackbox_state_t *view, keypos_t key, uint8_t col, uint8_t row) {
    char label[4];
    key_label(resolved_keycode(view->layers, key), view->flags & BB_GAMING, label);
    oled_set_cursor(col, row);
    oled_write(label, matrix_is_on(key.row, key.col));
}

static void render_blueprint(const blackbox_state_t *view) {
    bool    left   = is_keyboard_left();
    uint8_t offset = left ? 0 : 4;
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 6; ++col) {
            keypos_t key = {.row = offset + row, .col = left ? col : 5 - col};
            render_key(view, key, col * 7 / 2, row);
        }
    }

    uint8_t layer = get_highest_layer(view->layers);
    write_text(0, 3, layer == 1 ? PSTR("SYM") : layer == 2 ? PSTR("NAV") : PSTR("SYS"), true);
    write_text(4, 3, left ? PSTR("L") : PSTR("R"), false);
    write_text(6, 3, view->flags & BB_GAMING ? PSTR("G") : PSTR(" "), view->flags & BB_GAMING);
    for (uint8_t thumb = 0; thumb < 3; ++thumb) {
        keypos_t key = {.row = offset + 3, .col = left ? thumb + 3 : 5 - thumb};
        render_key(view, key, 8 + thumb * 4, 3);
    }
}

static void render_gauge(char name, uint8_t value, uint8_t col, uint8_t row) {
    oled_set_cursor(col, row);
    oled_write_char(name, false);
    write_number(col + 2, row, value);
    uint8_t filled = (uint16_t)value * 26 / 255;
    for (uint8_t x = 0; x < 28; ++x) {
        uint8_t pixels = x == 0 || x == 27 || x <= filled ? 0x7E : 0x42;
        oled_write_raw_byte((char)pixels, row * 128 + col * 6 + 32 + x);
    }
}

static void render_rgb(const blackbox_state_t *view) {
    write_text(0, 0, view->flags & BB_RGB_ON ? PSTR("RGB ON ") : PSTR("RGB OFF"), true);
    write_text(8, 0, PSTR("FX"), false);
    write_number(11, 0, view->rgb.mode);
    write_text(16, 0, PSTR("F"), false);
    write_number(18, 0, view->rgb.flags);
    render_gauge('H', view->rgb.hue, 0, 1);
    render_gauge('S', view->rgb.saturation, 11, 1);
    render_gauge('V', view->rgb.value, 0, 2);
    render_gauge('P', view->rgb.speed, 11, 2);
    write_text(0, 3, PSTR("HUE SAT VAL PACE"), false);
    write_text(18, 3, view->flags & BB_GAMING ? PSTR("ARM") : PSTR("   "), view->flags & BB_GAMING);
}

static void render_shutdown(uint32_t elapsed) {
    uint8_t half_height = (BLACKBOX_SHUTDOWN_MS - elapsed) * 15 / BLACKBOX_SHUTDOWN_MS;
    uint8_t half_width  = elapsed < BLACKBOX_SHUTDOWN_MS / 2 ? 64 : (BLACKBOX_SHUTDOWN_MS - elapsed) * 64 / (BLACKBOX_SHUTDOWN_MS / 2);
    uint8_t top         = 15 - half_height;
    uint8_t bottom      = 16 + half_height;

    oled_set_brightness((uint32_t)OLED_BRIGHTNESS * (BLACKBOX_SHUTDOWN_MS - elapsed) / BLACKBOX_SHUTDOWN_MS);
    for (uint8_t x = 0; x < 128; ++x) {
        uint32_t pixels = 0;
        if (x >= 64 - half_width && x < 64 + half_width) {
            pixels = (1UL << top) | (1UL << bottom);
            if (x == 64 - half_width || x == 63 + half_width) {
                pixels = (UINT32_MAX << top) & (UINT32_MAX >> (31 - bottom));
            }
        }
        for (uint8_t page = 0; page < 4; ++page) {
            oled_write_raw_byte((char)(pixels >> (page * 8)), page * 128 + x);
        }
    }
}

static blackbox_page_t select_page(const blackbox_state_t *view, uint32_t now, bool left) {
    uint32_t idle = view->flags & BB_KEY_HELD ? 0 : elapsed_since(now, view->activity_at);
    if (idle >= BLACKBOX_IDLE_MS + BLACKBOX_SHUTDOWN_MS) {
        return BB_PAGE_SLEEP;
    }
    if (idle >= BLACKBOX_IDLE_MS) {
        return BB_PAGE_SHUTDOWN;
    }
    if (!left && (view->flags & BB_RGB_RECENT) && elapsed_since(now, view->rgb_at) < BLACKBOX_RGB_MS) {
        return BB_PAGE_RGB;
    }
    if (get_highest_layer(view->layers) != 0) {
        return BB_PAGE_BLUEPRINT;
    }
    return left ? BB_PAGE_STATUS : BB_PAGE_TELEMETRY;
}

bool oled_task_user(void) {
    blackbox_state_t view;
    bool             valid;
    uint32_t         last_received;
    ATOMIC_BLOCK_RESTORESTATE {
        memcpy(&view, &state, sizeof(view));
        valid         = received_state;
        last_received = received_at;
    }

    uint32_t        now       = sync_timer_read32();
    bool            connected = is_keyboard_master() || (valid && timer_elapsed32(last_received) < BLACKBOX_LINK_TIMEOUT_MS);
    blackbox_page_t page      = select_page(&view, now, is_keyboard_left());
    if (!connected) {
        page = timer_elapsed32(last_received) >= BLACKBOX_IDLE_MS ? BB_PAGE_SLEEP : BB_PAGE_LINK;
    }
    bool history_changed = update_history(now, connected ? view.wpm : 0);
    bool page_changed    = page != current_page;

    if (page == BB_PAGE_SLEEP) {
        if (page_changed) {
            // Drain dirty blocks before switching off; otherwise the driver wakes itself.
            oled_clear();
            oled_render_dirty(true);
            oled_off();
            current_page = page;
        }
        return false;
    }

    if (page_changed) {
        oled_clear();
        current_page = page;
    }
    oled_on();
    if (page != BB_PAGE_SHUTDOWN) {
        oled_set_brightness(OLED_BRIGHTNESS);
    }

    switch (page) {
        case BB_PAGE_STATUS:
            render_status(&view);
            break;
        case BB_PAGE_TELEMETRY:
            render_telemetry(&view, now, page_changed || history_changed);
            break;
        case BB_PAGE_BLUEPRINT:
            render_blueprint(&view);
            break;
        case BB_PAGE_RGB:
            render_rgb(&view);
            break;
        case BB_PAGE_SHUTDOWN:
            render_shutdown(elapsed_since(now, view.activity_at) - BLACKBOX_IDLE_MS);
            break;
        case BB_PAGE_LINK:
            write_text(0, 0, PSTR("BLACKBOX // NO LINK"), true);
            write_text(0, 2, PSTR("CHECK SPLIT CABLE"), false);
            write_text(0, 3, PSTR("FLASH BOTH HALVES"), false);
            break;
        default:
            break;
    }
    return false;
}

#endif
