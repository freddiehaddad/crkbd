#pragma once

#ifdef OLED_ENABLE
    #define OLED_FONT_H "oled_font.c"
    #define OLED_FONT_START 32
    #define OLED_FONT_END 126
    #define OLED_BRIGHTNESS 96
    // Blackbox handles input-driven sleep, including the shutdown animation.
    #define OLED_TIMEOUT 0
    #define SPLIT_TRANSPORT_MIRROR
    #define SPLIT_TRANSACTION_IDS_USER BLACKBOX_SYNC
#endif

// Debounce time in milliseconds. Determines how long to wait for switch
// contacts to settle after detecting a state change. Default is 5ms.
// Increase this value if experiencing key chatter (duplicate keypresses).
// See: https://docs.qmk.fm/feature_debounce_type
// #define DEBOUNCE 10

#ifdef RGB_MATRIX_ENABLE
    #define RGB_MATRIX_SLEEP
    #define RGB_MATRIX_HUE_STEP 8
    #define RGB_MATRIX_SAT_STEP 8
    #define RGB_MATRIX_VAL_STEP 8
    #define RGB_MATRIX_SPD_STEP 10

    // #define ENABLE_RGB_MATRIX_SOLID_COLOR
    // #define ENABLE_RGB_MATRIX_ALPHAS_MODS
    #define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
    // #define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
    // #define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#endif
