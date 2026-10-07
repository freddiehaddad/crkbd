// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef OLED_ENABLE
void blackbox_note_key(uint16_t keycode);
void blackbox_set_gaming_mode(bool enabled);
#else
static inline void blackbox_note_key(uint16_t keycode) {
    (void)keycode;
}

static inline void blackbox_set_gaming_mode(bool enabled) {
    (void)enabled;
}
#endif
