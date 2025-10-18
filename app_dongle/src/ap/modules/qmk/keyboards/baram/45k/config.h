#pragma once


#define KBD_NAME                    "BARAM-45K-ESP32"

#define USB_VID                     0x0483
#define USB_PID                     0x5300

#define EECONFIG_USER_DATA_SIZE     0
#define TOTAL_EEPROM_BYTE_COUNT     2048

#define DYNAMIC_KEYMAP_LAYER_COUNT  8



#ifdef RF_DONGLE_MODE_ENABLE

// Key matrix dimensions
#define LEFT_COLS 6
#define LEFT_ROWS 4
#define RIGHT_COLS 6
#define RIGHT_ROWS 4

#define MATRIX_COLS (LEFT_COLS + RIGHT_COLS)
#define MATRIX_ROWS (LEFT_ROWS > RIGHT_ROWS ? LEFT_ROWS : RIGHT_ROWS)
#else

#define MATRIX_ROWS                 3
#define MATRIX_COLS                 2

#endif

#define DEBOUNCE                    5

// #define DEBUG_MATRIX_SCAN_RATE