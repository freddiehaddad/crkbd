// -----------------------------------------------------------------------------
// Prereqs (for reference; these live elsewhere)
//   rules.mk : KEY_OVERRIDE_ENABLE = yes
//              COMBO_ENABLE        = yes
//   config.h : (optional) #define COMBO_TERM 100  // widen chord timing window
//
// Docs:
//   Key Overrides: https://docs.qmk.fm/features/key_overrides
//   Combos:        https://docs.qmk.fm/features/combo
//   Magic keycodes (GUI toggle): https://docs.qmk.fm/keycodes_magic
// -----------------------------------------------------------------------------

#include QMK_KEYBOARD_H

// =========================
// Key Overrides
// =========================
//
// Key Overrides let you replace a specific "modifier + key" combination
// with a different output—without adding extra FN layers or custom logic.
// The ko_make_basic(mods, key, replacement) helper triggers when `key` is
// pressed together with *at least* the modifiers in `mods`. (Other modifiers
// may also be held unless you use the stricter helpers that limit layers/negmods.)

// Trigger: Shift + PIPE  =>  outputs '?' instead of '|'
// - MOD_MASK_SHIFT matches either left or right Shift.
// - KC_PIPE is the '|' key; KC_QUES is '?'.
// - Good for locale/layout tweaks or punctuation ergonomics.
const key_override_t pipe_key_override   = ko_make_basic(MOD_MASK_SHIFT, KC_PIPE, KC_QUES);

// Trigger: Shift + Backspace  =>  outputs Delete
// - A common ergonomic override so you don’t need a dedicated Delete key.
// - Works with either Shift (LShift or RShift).
const key_override_t delete_key_override = ko_make_basic(MOD_MASK_SHIFT, KC_BSPC, KC_DEL);

// NOTE:
// If you want to constrain an override to certain layers, or forbid extra
// modifiers from being held, prefer:
//   ko_make_with_layers(...)
//   ko_make_with_layers_and_negmods(...)
// See the Key Overrides docs for examples.


// =========================
// Combo: Win/GUI key lock
// =========================
//
// This combo toggles the OS "GUI" (Windows/Command) keys globally—handy
// for gaming or avoiding accidental Start/Menu opens.
//
// How combos work:
// - Combos match **keycodes**, not physical positions.
// - All keys in the combo must be pressed within COMBO_TERM (default ~50ms;
//   you can adjust in config.h).
// - Order of key presses doesn’t matter.
//
// Here we chord: MO(1) + MO(2) + ESC
//   MO(1) and MO(2) are momentary layer switches to layers 1 and 2.
//   KC_ESC is Escape.
// If your actual layer keys are LT(...) (layer‑tap) instead of MO(...),
// make sure to use those exact LT(...) keycodes here (combos must match
// the *concrete* keycode you press).

// Store combo sequence in flash (AVR) via PROGMEM; harmless on ARM.
const uint16_t PROGMEM winlock_combo[] = {
    MO(1), MO(2), KC_ESC, COMBO_END,
};

// Map the combo to the Magic keycode that toggles GUI keys on/off.
combo_t key_combos[] = {
    COMBO(winlock_combo, QK_MAGIC_TOGGLE_GUI), // alias: GU_TOGG
};

// TIPs:
// - If pressing MO(1)+MO(2) changes the layer where ESC becomes transparent,
//   consider adding `#define COMBO_ONLY_FROM_LAYER _BASE` in config.h so the
//   combo is evaluated from your base layer’s keycodes.
// - If the 3-key chord feels tight, raise COMBO_TERM (e.g., 80–120ms).

// =========================
// Register the overrides
// =========================
//
// This global pointer list tells QMK which overrides to apply.
// Some QMK versions expect this array to be NULL-terminated.
// If your toolchain warns about that, add a trailing `NULL`.

// This globally defines all key overrides to be used
const key_override_t *key_overrides[] = {
	&pipe_key_override,
    &delete_key_override,
    // NULL  // <-- uncomment if your QMK version requires NULL termination
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,                         KC_Y,    KC_U,    KC_I,    KC_O,   KC_P,  KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,                         KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LALT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,                         KC_N,    KC_M, KC_COMM,  KC_DOT, KC_PIPE,  KC_ESC,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI,   MO(1),  KC_SPC,     KC_ENT,   MO(2), KC_LSFT
                                      //`--------------------------'  `--------------------------'

  ),

    [1] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      KC_ASTR, KC_EXLM,   KC_AT, KC_HASH,  KC_DLR, KC_PERC,                      KC_CIRC, KC_AMPR, KC_ASTR, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, KC_LABK, KC_LPRN, KC_LBRC, KC_LCBR, KC_BSLS,                      KC_SLSH, KC_RCBR, KC_RBRC, KC_RPRN, KC_RABK,  KC_GRV,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LALT, XXXXXXX, XXXXXXX, XXXXXXX,    KC_6,    KC_7,                      KC_UNDS, KC_MINS, KC_PLUS,  KC_EQL, XXXXXXX, KC_TILD,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT,   MO(3), KC_LSFT
                                      //`--------------------------'  `--------------------------'
  ),

    [2] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                         KC_6,    KC_7,    KC_8,    KC_9,    KC_0, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, XXXXXXX, XXXXXXX, XXXXXXX, C(KC_SPC), XXXXXXX,                    KC_LEFT, KC_DOWN,   KC_UP, KC_RGHT, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LALT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_HOME, KC_PGDN, KC_PGUP,  KC_END, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI,   MO(3),  KC_SPC,     KC_ENT, _______, KC_LSFT
                                      //`--------------------------'  `--------------------------'
  ),

    [3] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      QK_BOOT, RM_TOGG, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                        KC_F1,   KC_F2,   KC_F3,   KC_F4, S(KC_PSCR), KC_PSCR,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, RM_NEXT, RM_HUEU, RM_SATU, RM_VALU, RM_SPDU,                        KC_F5,   KC_F6,   KC_F7,   KC_F8,  XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LALT, RM_PREV, RM_HUED, RM_SATD, RM_VALD, RM_SPDD,                        KC_F9,   KC_F10,  KC_F11,  KC_F12, XXXXXXX, KC_ESC,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_LSFT
                                      //`--------------------------'  `--------------------------'
  )
};
