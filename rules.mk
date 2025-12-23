# RGB underglow (classic "rgblight") feature. Provides strip-style RGB effects
# (typically underglow) using addressable LEDs like WS2812/SK6812. Turned off
# here because you're using the newer RGB Matrix system instead.
# Docs: https://docs.qmk.fm/features/rgblight
RGBLIGHT_ENABLE = no

# RGB Matrix (per‑key/per‑zone RGB) with rich effects and per‑LED control. Many
# keyboards set the required driver at the keyboard level (e.g., IS31FL37xx or
# WS2812). If needed in your keymap, also set: RGB_MATRIX_DRIVER = <driver>.
# Docs: https://docs.qmk.fm/features/rgb_matrix
RGB_MATRIX_ENABLE = yes

# Key Overrides: remap specific modifier+key combos to alternate output (e.g.,
# Shift+Backspace -> Delete, Ctrl+VolUp -> BrightnessUp, etc.). Define
# overrides in keymap.c via the key_overrides[] array and helpers like
# ko_make_basic().
# Docs: https://docs.qmk.fm/features/key_overrides
KEY_OVERRIDE_ENABLE = yes

# Combos: press a chord of keys within COMBO_TERM to emit another
# action/keycode (supports advanced keycodes like LT(), mod‑taps, tap‑dance).
# Define combos in keymap.c (combo_t key_combos[]).
# Docs: https://docs.qmk.fm/features/combo
COMBO_ENABLE = yes

# Debounce Algorithm: controls how key state changes are filtered to prevent
# switch contact bounce/chatter. sym_defer_pk uses per-key timing (each key
# gets its own debounce timer) and waits for state to stabilize before
# reporting changes. More noise-resistant than global debouncing but uses
# more RAM. Also set DEBOUNCE in config.h to adjust the debounce time (ms).
# Docs: https://docs.qmk.fm/features/debounce_type
DEBOUNCE_TYPE = sym_defer_pk
