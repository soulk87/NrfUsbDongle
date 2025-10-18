// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "action_layer.h"
// LVGL 레이어 업데이트 함수 (ap_lvgl.c에 정의됨)
extern void apLvglUpdateLayer(uint8_t layer);



const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
    * Layer 0 (Base Layer - QWERTY)
    * ,-----------------------------------------------------------------------------------.
    * | Tab  |   Q  |   W  |   E  |   R  |   T  |   Y  |   U  |   I  |   O  |   P  | Bksp |
    * |------+------+------+------+------+------+------+------+------+------+------+------|
    * | Esc  |   A  |   S  |   D  |   F  |   G  |   H  |   J  |   K  |   L  |   ;  |  '   |
    * |------+------+------+------+------+------+------+------+------+------+------+------|
    * | Shift|   Z  |   X  |   C  |   V  |   B  |   N  |   M  |   ,  |   .  |   /  |Enter |
    * |------+------+------+------+------+------+------+------+------+------+------+------|
    * |  NO  |  NO  |  NO  |Lower | Ctrl | Space| Alt  |Raise | Left |  NO  |  NO  |  NO  |
    * `-----------------------------------------------------------------------------------'
    */
    [0] = {
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_BSPC},
        {KC_ESC,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT},
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_ENT },
        {KC_NO,   KC_NO,   KC_NO,   MO(1),   KC_LCTL, KC_SPC,  KC_LALT, MO(2),   KC_LEFT, KC_NO,   KC_NO,   KC_NO  }
    },
    
};
volatile uint32_t test_keymap_c = 0;

/**
 * @brief 레이어 상태 변경 콜백
 * 레이어가 변경될 때마다 LVGL 디스플레이를 업데이트합니다.
 */
layer_state_t layer_state_set_user(layer_state_t state)
{
    uint8_t layer = get_highest_layer(state);
    apLvglUpdateLayer(layer);

    return state;   
}