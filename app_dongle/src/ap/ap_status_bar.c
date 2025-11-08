#include "ap_status_bar.h"
#include "my_key_protocol.h"
#include <stdlib.h>
#include "usb.h"

// 상태바 데이터 구조체
typedef struct {
  bool left_connected;
  bool right_connected;
  uint8_t left_battery;
  uint8_t right_battery;
  bool usb_connected;
} status_bar_data_t;

// UI 객체들 - 상태바
static lv_obj_t *status_bar;
static lv_obj_t *left_kb_icon;      // 왼쪽 키보드 아이콘 컨테이너
static lv_obj_t *left_bat_bar;      // 왼쪽 배터리 바
static lv_obj_t *left_bat_fill;     // 왼쪽 배터리 채우기

static lv_obj_t *right_kb_icon;     // 오른쪽 키보드 아이콘 컨테이너
static lv_obj_t *right_bat_bar;     // 오른쪽 배터리 바
static lv_obj_t *right_bat_fill;    // 오른쪽 배터리 채우기

static lv_obj_t *left_status_led;   // 왼쪽 연결 상태 LED (초록/빨강)
static lv_obj_t *usb_led;           // USB LED (초록/빨강)
static lv_obj_t *right_status_led;  // 오른쪽 연결 상태 LED (초록/빨강)

// 상태바 데이터
static status_bar_data_t status_data = {
  .left_connected = false,
  .right_connected = false,
  .left_battery = 0,
  .right_battery = 0,
  .usb_connected = false
};

// 타이머
static lv_timer_t *status_check_timer = NULL;

// 함수 프로토타입
static void check_wireless_status(lv_timer_t *timer);

/**
 * @brief 상태바 생성 (그래픽 기반)
 */
void apStatusBarCreate(void)
{
  // 상단 상태바 컨테이너 (더 낮게)
  status_bar = lv_obj_create(lv_scr_act());
  lv_obj_set_size(status_bar, 240, 30);
  lv_obj_set_pos(status_bar, 0, 0);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_radius(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 0, 0);
  lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // ============ 왼쪽 키보드 (왼쪽 상단) ============
  left_kb_icon = lv_obj_create(status_bar);
  lv_obj_set_size(left_kb_icon, 60, 26);
  lv_obj_set_pos(left_kb_icon, 2, 2);
  lv_obj_set_style_bg_color(left_kb_icon, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_color(left_kb_icon, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(left_kb_icon, 1, 0);
  lv_obj_set_style_radius(left_kb_icon, 3, 0);
  lv_obj_set_style_pad_all(left_kb_icon, 2, 0);
  lv_obj_clear_flag(left_kb_icon, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // 왼쪽 배터리 외곽선 (짧고 두껍게, 중앙 정렬)
  left_bat_bar = lv_obj_create(left_kb_icon);
  lv_obj_set_size(left_bat_bar, 45, 14);
  lv_obj_set_pos(left_bat_bar, 5, 6);
  lv_obj_set_style_bg_color(left_bat_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_color(left_bat_bar, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(left_bat_bar, 2, 0);
  lv_obj_set_style_radius(left_bat_bar, 2, 0);
  lv_obj_set_style_pad_all(left_bat_bar, 2, 0);
  
  // 왼쪽 배터리 채우기
  left_bat_fill = lv_obj_create(left_bat_bar);
  lv_obj_set_size(left_bat_fill, 0, 10);  // 초기 0%, 두껍게
  lv_obj_set_pos(left_bat_fill, 0, 0);
  lv_obj_set_style_bg_color(left_bat_fill, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(left_bat_fill, 0, 0);
  lv_obj_set_style_radius(left_bat_fill, 1, 0);
  
  // 왼쪽 배터리 팁
  lv_obj_t *left_bat_tip = lv_obj_create(left_kb_icon);
  lv_obj_set_size(left_bat_tip, 2, 6);
  lv_obj_set_pos(left_bat_tip, 51, 10);
  lv_obj_set_style_bg_color(left_bat_tip, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(left_bat_tip, 0, 0);
  lv_obj_set_style_radius(left_bat_tip, 1, 0);
  
  // ============ 중앙 상태 LED 3개 ============
  // 왼쪽 연결 상태 LED (초록/빨강)
  left_status_led = lv_obj_create(status_bar);
  lv_obj_set_size(left_status_led, 10, 10);
  lv_obj_align(left_status_led, LV_ALIGN_TOP_MID, -15, 10);
  lv_obj_set_style_bg_color(left_status_led, lv_color_hex(0xFF0000), 0);  // 기본 빨강
  lv_obj_set_style_border_width(left_status_led, 0, 0);
  lv_obj_set_style_radius(left_status_led, LV_RADIUS_CIRCLE, 0);
  
  // USB LED (초록/빨강)
  usb_led = lv_obj_create(status_bar);
  lv_obj_set_size(usb_led, 10, 10);
  lv_obj_align(usb_led, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_bg_color(usb_led, lv_color_hex(0xFF0000), 0);  // 기본 빨강
  lv_obj_set_style_border_width(usb_led, 0, 0);
  lv_obj_set_style_radius(usb_led, LV_RADIUS_CIRCLE, 0);
  
  // 오른쪽 연결 상태 LED (초록/빨강)
  right_status_led = lv_obj_create(status_bar);
  lv_obj_set_size(right_status_led, 10, 10);
  lv_obj_align(right_status_led, LV_ALIGN_TOP_MID, 15, 10);
  lv_obj_set_style_bg_color(right_status_led, lv_color_hex(0xFF0000), 0);  // 기본 빨강
  lv_obj_set_style_border_width(right_status_led, 0, 0);
  lv_obj_set_style_radius(right_status_led, LV_RADIUS_CIRCLE, 0);
  
  // ============ 오른쪽 키보드 (오른쪽 상단) ============
  right_kb_icon = lv_obj_create(status_bar);
  lv_obj_set_size(right_kb_icon, 60, 26);
  lv_obj_align(right_kb_icon, LV_ALIGN_TOP_RIGHT, -2, 2);
  lv_obj_set_style_bg_color(right_kb_icon, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_color(right_kb_icon, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(right_kb_icon, 1, 0);
  lv_obj_set_style_radius(right_kb_icon, 3, 0);
  lv_obj_set_style_pad_all(right_kb_icon, 2, 0);
  lv_obj_clear_flag(right_kb_icon, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // 오른쪽 배터리 외곽선 (짧고 두껍게, 중앙 정렬)
  right_bat_bar = lv_obj_create(right_kb_icon);
  lv_obj_set_size(right_bat_bar, 45, 14);
  lv_obj_set_pos(right_bat_bar, 3, 6);
  lv_obj_set_style_bg_color(right_bat_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_color(right_bat_bar, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(right_bat_bar, 2, 0);
  lv_obj_set_style_radius(right_bat_bar, 2, 0);
  lv_obj_set_style_pad_all(right_bat_bar, 2, 0);
  
  // 오른쪽 배터리 채우기
  right_bat_fill = lv_obj_create(right_bat_bar);
  lv_obj_set_size(right_bat_fill, 0, 10);  // 초기 0%, 두껍게
  lv_obj_set_pos(right_bat_fill, 0, 0);
  lv_obj_set_style_bg_color(right_bat_fill, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(right_bat_fill, 0, 0);
  lv_obj_set_style_radius(right_bat_fill, 1, 0);
  
  // 오른쪽 배터리 팁
  lv_obj_t *right_bat_tip = lv_obj_create(right_kb_icon);
  lv_obj_set_size(right_bat_tip, 2, 6);
  lv_obj_set_pos(right_bat_tip, 49, 10);
  lv_obj_set_style_bg_color(right_bat_tip, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(right_bat_tip, 0, 0);
  lv_obj_set_style_radius(right_bat_tip, 1, 0);
  
  // 타이머 생성 (1초마다 무선 통신 상태 확인)
  status_check_timer = lv_timer_create(check_wireless_status, 1000, NULL);
}

/**
 * @brief 무선 통신 상태 확인 및 업데이트 (타이머 콜백)
 */
static void check_wireless_status(lv_timer_t *timer)
{
  ARG_UNUSED(timer);
  
  // key_protocol에서 모든 상태를 한 번에 가져오기 (thread-safe)
  bool left_conn, right_conn;
  uint8_t left_bat, right_bat;
  
  key_protocol_get_all_states(&left_conn, &left_bat, &right_conn, &right_bat);
  
  // 왼쪽 키보드 업데이트
  apStatusBarUpdateLeftKeyboard(left_conn, left_bat);
  
  // 오른쪽 키보드 업데이트
  apStatusBarUpdateRightKeyboard(right_conn, right_bat);
  
  // USB 연결 상태 업데이트
  apStatusBarUpdateUSB(usbIsConnect());
}

/**
 * @brief 왼쪽 키보드 연결 상태 업데이트
 */
void apStatusBarUpdateLeftKeyboard(bool connected, uint8_t battery)
{
  status_data.left_connected = connected;
  status_data.left_battery = battery;
  
  if (connected)
  {
    // 연결 상태 LED: 초록
    lv_obj_set_style_bg_color(left_status_led, lv_color_hex(0x00FF00), 0);
    
    // 배터리 색상
    uint32_t bat_color;
    if (battery <= 20) {
      bat_color = 0xFF0000;  // 빨강
    } else if (battery <= 50) {
      bat_color = 0xFFAA00;  // 노랑
    } else {
      bat_color = 0x00FF00;  // 녹색
    }
    lv_obj_set_style_bg_color(left_bat_fill, lv_color_hex(bat_color), 0);
    
    // 배터리 바 너비 조정 (최대 41px - 짧아진 배터리에 맞춤)
    int16_t bat_width = (battery * 41) / 100;
    lv_obj_set_width(left_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨강
    lv_obj_set_style_bg_color(left_status_led, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(left_bat_fill, 0);
  }
}

/**
 * @brief 오른쪽 키보드 연결 상태 업데이트
 */
void apStatusBarUpdateRightKeyboard(bool connected, uint8_t battery)
{
  status_data.right_connected = connected;
  status_data.right_battery = battery;
  
  if (connected)
  {
    // 연결 상태 LED: 초록
    lv_obj_set_style_bg_color(right_status_led, lv_color_hex(0x00FF00), 0);
    
    // 배터리 색상
    uint32_t bat_color;
    if (battery <= 20) {
      bat_color = 0xFF0000;  // 빨강
    } else if (battery <= 50) {
      bat_color = 0xFFAA00;  // 노랑
    } else {
      bat_color = 0x00FF00;  // 녹색
    }
    lv_obj_set_style_bg_color(right_bat_fill, lv_color_hex(bat_color), 0);
    
    // 배터리 바 너비 조정 (최대 41px - 짧아진 배터리에 맞춤)
    int16_t bat_width = (battery * 41) / 100;
    lv_obj_set_width(right_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨강
    lv_obj_set_style_bg_color(right_status_led, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(right_bat_fill, 0);
  }
}

/**
 * @brief USB 연결 상태 업데이트
 */
void apStatusBarUpdateUSB(bool connected)
{
  status_data.usb_connected = connected;
  
  if (connected)
  {
    // USB 연결됨: 초록
    lv_obj_set_style_bg_color(usb_led, lv_color_hex(0x00FF00), 0);
  }
  else
  {
    // USB 연결 끊김: 빨강
    lv_obj_set_style_bg_color(usb_led, lv_color_hex(0xFF0000), 0);
  }
}
