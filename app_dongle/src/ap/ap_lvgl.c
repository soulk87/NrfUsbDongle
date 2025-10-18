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
#define KEY_SPACING 4

// 색상 정의
#define COLOR_KEY_BG        0x2a2a2a
#define COLOR_KEY_BORDER    0x555555
#define COLOR_TEXT          0xFFFFFF
#define COLOR_LAYER_TEXT    0x00AAFF

// 전역 변수
static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);
static struct k_thread lvgl_thread_data;
static k_tid_t lvgl_thread_id = NULL;

// UI 객체들
static lv_obj_t *main_screen;
static lv_obj_t *info_label;

// 타이머
static lv_timer_t *update_timer = NULL;

// 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);
static void create_main_screen(void);
static void update_display(lv_timer_t *timer);

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
  
  // 정보 표시 라벨 (중앙)
  info_label = lv_label_create(main_screen);
  lv_label_set_text(info_label, "Ready");
  lv_obj_set_style_text_color(info_label, lv_color_hex(COLOR_TEXT), 0);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
  lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief 디스플레이 업데이트 (타이머 콜백)
 */
static void update_display(lv_timer_t *timer)
{
  ARG_UNUSED(timer);
  
  // TODO: 여기에 업데이트 로직 추가
}

/**
 * @brief LVGL 초기화 및 메인 화면 생성
 */
static void lvgl_main_init(void)
{
  lv_init();
  
  // 메인 스크린 배경 설정
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  
  // 상태바 생성
  apStatusBarCreate();
  
  // 메인 화면 생성
  create_main_screen();
  
  // 타이머 생성 (500ms마다 업데이트)
  update_timer = lv_timer_create(update_display, 500, NULL);
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
 */
void apLvglUpdateLayer(uint8_t layer)
{
  // TODO: 레이어 변경에 따른 UI 업데이트 로직 추가
  // 현재는 info_label에 레이어 번호만 표시
  char layer_text[32];
  snprintf(layer_text, sizeof(layer_text), "Layer: %d", layer);
  if (info_label != NULL) {
    lv_label_set_text(info_label, layer_text);
  }
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
