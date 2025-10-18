#include "ap_lvgl.h"
#include "ap_status_bar.h"
#include "ap.h"
#include "lvgl/lvgl.h"
#include <zephyr/kernel.h>
#include <stdio.h>

// QMK 헤더 포함
#include "quantum/quantum.h"
#include "quantum/action_layer.h"
#include "quantum/keycode.h"
#include "quantum/dynamic_keymap.h"
#include "quantum/action_util.h"
#include "quantum/modifiers.h"
#include "keyboards/baram/45k/config.h"

#define LVGL_THREAD_STACK_SIZE 4096
#define LVGL_THREAD_PRIORITY 6

// config.h에서 정의된 매크로 사용
#if defined(RF_DONGLE_MODE_ENABLE)
// Dongle 모드: LEFT_COLS, LEFT_ROWS, RIGHT_COLS, RIGHT_ROWS, MATRIX_COLS, MATRIX_ROWS 이미 정의됨
#else
// 일반 키보드 모드
#endif

#define MAX_LAYERS DYNAMIC_KEYMAP_LAYER_COUNT

// 키 버튼 설정
#define KEY_SPACING 2
#define KEY_WIDTH 16
#define KEY_HEIGHT 32

// 색상 정의
#define COLOR_KEY_BG        0x2a2a2a
#define COLOR_KEY_BORDER    0x555555
#define COLOR_TEXT          0xFFFFFF
#define COLOR_LAYER_TEXT    0x00AAFF

// 키보드 레이아웃 정의 (HKH_DongleKeyboard.JSON 기반)
// Row 0-2: 6키 + 간격 + 6키
// Row 3: 3키 간격 후 3키 + 간격 + 3키
#define LEFT_KEYS_PER_ROW 6
#define RIGHT_KEYS_PER_ROW 6
#define SPLIT_GAP 6  // 중앙 간격 (픽셀)

// 전역 변수
static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);
static struct k_thread lvgl_thread_data;
static k_tid_t lvgl_thread_id = NULL;

// UI 객체들
static lv_obj_t *main_screen;
static lv_obj_t *layer_label;
static lv_obj_t *key_buttons[MATRIX_ROWS][MATRIX_COLS];

// 타이머
static lv_timer_t *update_timer = NULL;

// 레이어 상태 관리 (멀티스레드 안전)
static struct k_mutex layer_mutex;
static volatile uint8_t current_layer = 0;
static volatile uint8_t pending_layer = 0;
static volatile bool layer_update_pending = false;

// Modifier 상태 관리
static uint8_t last_mods = 0;

// 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);
static void create_main_screen(void);
static void update_display(lv_timer_t *timer);
static void create_key_button(uint8_t row, uint8_t col, int16_t x, int16_t y);
static const char* keycode_to_string(uint16_t keycode);
static void process_layer_update(void);

/**
 * @brief Keycode를 문자열로 변환
 */
static const char* keycode_to_string(uint16_t keycode)
{
  // Shift 키 상태 확인
  uint8_t mods = get_mods();
  bool shift_pressed = (mods & MOD_MASK_SHIFT) != 0;
  
  // 기본 키코드 매핑
  if (keycode >= KC_A && keycode <= KC_Z) {
    static char key_char[2] = {0};
    key_char[0] = 'A' + (keycode - KC_A);
    return key_char;
  }
  
  // 숫자 키: Shift 눌림 상태에 따라 다른 문자 표시
  if (keycode >= KC_1 && keycode <= KC_0) {
    if (shift_pressed) {
      // Shift + 숫자 = 특수 문자
      static const char* shifted_nums[] = {
        "!", "@", "#", "$", "%", "^", "&", "*", "(", ")"
      };
      int index = (keycode == KC_0) ? 9 : (keycode - KC_1);
      return shifted_nums[index];
    } else {
      // 일반 숫자
      static char num_char[2] = {0};
      num_char[0] = (keycode == KC_0) ? '0' : ('1' + (keycode - KC_1));
      return num_char;
    }
  }
  
  // 기타 특수 키들도 Shift 상태 반영
  switch (keycode) {
    case KC_NO: return "";
    case KC_ESC: return "Esc";
    case KC_TAB: return "Tab";
    case KC_SPC: return "Spc";
    case KC_ENT: return "Ent";
    case KC_BSPC: return "Bk";
    case KC_LSFT: return "Sft";
    case KC_LCTL: return "Ctl";
    case KC_LALT: return "Alt";
    case KC_LGUI: return "Gui";
    case KC_LEFT: return "←";
    case KC_DOWN: return "↓";
    case KC_UP: return "↑";
    case KC_RGHT: return "→";
    case KC_SCLN: return shift_pressed ? ":" : ";";
    case KC_QUOT: return shift_pressed ? "\"" : "'";
    case KC_COMM: return shift_pressed ? "<" : ",";
    case KC_DOT: return shift_pressed ? ">" : ".";
    case KC_SLSH: return shift_pressed ? "?" : "/";
    case KC_GRV: return shift_pressed ? "~" : "`";
    case KC_MINS: return shift_pressed ? "_" : "-";
    case KC_EQL: return shift_pressed ? "+" : "=";
    case KC_LBRC: return shift_pressed ? "{" : "[";
    case KC_RBRC: return shift_pressed ? "}" : "]";
    case KC_BSLS: return shift_pressed ? "|" : "\\";
    case KC_CAPS: return "Cap";
    case KC_DEL: return "Del";
    case KC_HOME: return "Hom";
    case KC_END: return "End";
    case KC_PGUP: return "PgU";
    case KC_PGDN: return "PgD";
    default:
      // 레이어 전환 키
      if (keycode >= QK_MOMENTARY && keycode <= QK_MOMENTARY_MAX) {
        static char mo_str[4];
        snprintf(mo_str, sizeof(mo_str), "L%d", keycode - QK_MOMENTARY);
        return mo_str;
      }
      return "?";
  }
}

/**
 * @brief 개별 키 버튼 생성
 */
static void create_key_button(uint8_t row, uint8_t col, int16_t x, int16_t y)
{
  // EEPROM에서 동적 키맵 가져오기
  uint16_t keycode = dynamic_keymap_get_keycode(0, row, col);
  
  // KC_NO(빈 키)는 생성하지 않음
  if (keycode == KC_NO) {
    key_buttons[row][col] = NULL;
    return;
  }
  
  // 버튼 생성
  key_buttons[row][col] = lv_obj_create(main_screen);
  lv_obj_t *btn = key_buttons[row][col];
  
  // 위치 및 크기 설정
  lv_obj_set_size(btn, KEY_WIDTH, KEY_HEIGHT);
  lv_obj_set_pos(btn, x, y);
  
  // 스타일 설정
  lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_KEY_BG), 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_KEY_BORDER), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_radius(btn, 3, 0);
  lv_obj_set_style_pad_all(btn, 1, 0);
  
  // 키 라벨 생성
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, keycode_to_string(keycode));
  lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief 메인 화면 생성
 */
static void create_main_screen(void)
{
  // 메인 화면 컨테이너 (상태바 30px 아래)
  main_screen = lv_obj_create(lv_scr_act());
  lv_obj_set_size(main_screen, 240, 210);
  lv_obj_set_pos(main_screen, 0, 30);
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(main_screen, 0, 0);
  lv_obj_set_style_radius(main_screen, 0, 0);
  lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
  
  // 레이어 표시 라벨
  layer_label = lv_label_create(main_screen);
  lv_label_set_text(layer_label, "Layer 0");
  lv_obj_set_style_text_color(layer_label, lv_color_hex(COLOR_LAYER_TEXT), 0);
  lv_obj_set_style_text_font(layer_label, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(layer_label, 5, 2);
  
  // 버튼 배열 초기화
  for (int row = 0; row < MATRIX_ROWS; row++) {
    for (int col = 0; col < MATRIX_COLS; col++) {
      key_buttons[row][col] = NULL;
    }
  }
  
  // 키보드 레이아웃 생성 (HKH_DongleKeyboard.JSON 기반)
  // 시작 위치 계산 (화면 중앙 정렬)
  int16_t start_x = 0;
  int16_t start_y = 20;
  
  // Row 0-2: 6키 + 간격 + 6키
  for (uint8_t row = 0; row < 3; row++) {
    int16_t y = start_y + row * (KEY_HEIGHT + KEY_SPACING);
    
    // 왼쪽 6개 키 (0,0 ~ 0,5)
    for (uint8_t col = 0; col < LEFT_KEYS_PER_ROW; col++) {
      int16_t x = start_x + col * (KEY_WIDTH + KEY_SPACING);
      create_key_button(row, col, x, y);
    }
    
    // 오른쪽 6개 키 (0,6 ~ 0,11)
    for (uint8_t col = LEFT_KEYS_PER_ROW; col < MATRIX_COLS; col++) {
      int16_t x = start_x + col * (KEY_WIDTH + KEY_SPACING) + SPLIT_GAP;
      create_key_button(row, col, x, y);
    }
  }
  
  // Row 3: 3개 간격 후 3키 + 간격 + 3키
  int16_t y = start_y + 3 * (KEY_HEIGHT + KEY_SPACING);
  
  // 왼쪽 3개 키 (3,3 ~ 3,5) - 3개 키만큼 오른쪽으로 시작
  for (uint8_t col = 3; col < 6; col++) {
    int16_t x = start_x + col * (KEY_WIDTH + KEY_SPACING);
    create_key_button(3, col, x, y);
  }
  
  // 오른쪽 3개 키 (3,6 ~ 3,8)
  for (uint8_t col = 6; col < 9; col++) {
    int16_t x = start_x + col * (KEY_WIDTH + KEY_SPACING) + SPLIT_GAP;
    create_key_button(3, col, x, y);
  }
}

/**
 * @brief 레이어 업데이트 처리 (LVGL 스레드에서만 호출)
 */
static void process_layer_update(void)
{
  k_mutex_lock(&layer_mutex, K_FOREVER);
  
  if (!layer_update_pending) {
    k_mutex_unlock(&layer_mutex);
    return;
  }
  
  uint8_t new_layer = pending_layer;
  uint8_t old_layer = current_layer;
  
  // 레이어 라벨 업데이트
  char layer_text[16];
  snprintf(layer_text, sizeof(layer_text), "Layer %d", new_layer);
  if (layer_label != NULL) {
    lv_label_set_text(layer_label, layer_text);
  }
  
  // 변경된 키만 업데이트
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      if (key_buttons[row][col] != NULL) {
        // KC_TRANSPARENT를 고려하여 실제 키코드 가져오기
        uint16_t old_keycode = KC_NO;
        for (int8_t layer = old_layer; layer >= 0; layer--) {
          old_keycode = dynamic_keymap_get_keycode(layer, row, col);
          if (old_keycode != KC_TRANSPARENT) {
            break;
          }
        }
        
        uint16_t new_keycode = KC_NO;
        for (int8_t layer = new_layer; layer >= 0; layer--) {
          new_keycode = dynamic_keymap_get_keycode(layer, row, col);
          if (new_keycode != KC_TRANSPARENT) {
            break;
          }
        }
        
        // 실제 표시되는 키코드가 변경된 경우에만 업데이트
        if (old_keycode != new_keycode) {
          lv_obj_t *label = lv_obj_get_child(key_buttons[row][col], 0);
          if (label != NULL) {
            lv_label_set_text(label, keycode_to_string(new_keycode));
          }
        }
      }
    }
  }
  
  current_layer = new_layer;
  layer_update_pending = false;
  
  k_mutex_unlock(&layer_mutex);
}

/**
 * @brief 디스플레이 업데이트 (타이머 콜백)
 */
static void update_display(lv_timer_t *timer)
{
  ARG_UNUSED(timer);
  
  // 레이어 업데이트가 대기 중이면 처리
  if (layer_update_pending) {
    process_layer_update();
  }
  
  // Modifier 상태 변화 감지 (Shift 키 등)
  uint8_t current_mods = get_mods();
  if (current_mods != last_mods) {
    // Shift 상태가 변경되었으면 모든 키 라벨 업데이트
    bool last_shift = (last_mods & MOD_MASK_SHIFT) != 0;
    bool current_shift = (current_mods & MOD_MASK_SHIFT) != 0;
    
    if (last_shift != current_shift) {
      // 현재 레이어의 모든 키 업데이트
      for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
          if (key_buttons[row][col] != NULL) {
            // KC_TRANSPARENT를 고려하여 실제 키코드 가져오기
            uint16_t keycode = KC_NO;
            for (int8_t layer = current_layer; layer >= 0; layer--) {
              keycode = dynamic_keymap_get_keycode(layer, row, col);
              if (keycode != KC_TRANSPARENT) {
                break;
              }
            }
            
            // 키 라벨 업데이트
            lv_obj_t *label = lv_obj_get_child(key_buttons[row][col], 0);
            if (label != NULL) {
              lv_label_set_text(label, keycode_to_string(keycode));
            }
          }
        }
      }
    }
    
    last_mods = current_mods;
  }
}

/**
 * @brief LVGL 초기화 및 메인 화면 생성
 */
static void lvgl_main_init(void)
{
  lv_init();
  
  // Mutex 초기화
  k_mutex_init(&layer_mutex);
  
  // 메인 스크린 배경 설정
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  
  // 상태바 생성
  apStatusBarCreate();
  
  // 메인 화면 생성
  create_main_screen();
  
  // 타이머 생성 (100ms마다 업데이트 - Shift 키 등의 반응성 향상)
  update_timer = lv_timer_create(update_display, 100, NULL);
}

/**
 * @brief LVGL 스레드 함수
 */
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);
  
  // LVGL 초기화
  lvgl_main_init();
  
  while (1)
  {
    lv_timer_handler();
    k_sleep(K_MSEC(5)); // 5ms 간격으로 LVGL 타이머 처리
  }
}

/**
 * @brief LVGL 초기화
 */
void apLvglInit(void)
{
  // 초기화 코드가 필요한 경우 여기에 추가
}

/**
 * @brief LVGL 스레드 시작
 */
void apLvglStart(void)
{
  // LVGL 스레드 생성
  lvgl_thread_id = k_thread_create(&lvgl_thread_data, lvgl_thread_stack,
                                   K_THREAD_STACK_SIZEOF(lvgl_thread_stack),
                                   lvgl_thread_func, NULL, NULL, NULL,
                                   LVGL_THREAD_PRIORITY, 0, K_NO_WAIT);
}

/**
 * @brief 레이어 변경 알림 (QMK에서 호출)
 * @note 다른 스레드(QMK)에서 호출되므로 LVGL API를 직접 사용하지 않음
 *       대신 플래그를 설정하고 LVGL 스레드에서 처리하도록 함
 */
void apLvglUpdateLayer(uint8_t layer)
{
  if (layer >= MAX_LAYERS) return;
  
  // Mutex를 즉시 획득할 수 없으면 그냥 무시하고 넘어감
  if (k_mutex_lock(&layer_mutex, K_NO_WAIT) != 0) {
    return;
  }
  
  pending_layer = layer;
  layer_update_pending = true;
  k_mutex_unlock(&layer_mutex);
}

/**
 * @brief 왼쪽 키보드 연결 상태 업데이트
 */
void apLvglUpdateLeftKeyboard(bool connected, uint8_t battery)
{
  apStatusBarUpdateLeftKeyboard(connected, battery);
}

/**
 * @brief 오른쪽 키보드 연결 상태 업데이트
 */
void apLvglUpdateRightKeyboard(bool connected, uint8_t battery)
{
  apStatusBarUpdateRightKeyboard(connected, battery);
}

/**
 * @brief USB 연결 상태 업데이트
 */
void apLvglUpdateUSB(bool connected)
{
  apStatusBarUpdateUSB(connected);
}
