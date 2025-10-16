#include "ap_status_bar.h"
#include <stdlib.h>

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
static lv_obj_t *left_status_dot;   // 왼쪽 연결 상태 점

static lv_obj_t *right_kb_icon;     // 오른쪽 키보드 아이콘 컨테이너
static lv_obj_t *right_bat_bar;     // 오른쪽 배터리 바
static lv_obj_t *right_bat_fill;    // 오른쪽 배터리 채우기
static lv_obj_t *right_status_dot;  // 오른쪽 연결 상태 점

static lv_obj_t *usb_icon;          // USB 아이콘
static lv_obj_t *usb_plug;          // USB 플러그 부분
static lv_obj_t *usb_body;          // USB 본체 부분

// 상태바 데이터
static status_bar_data_t status_data = {
  .left_connected = false,
  .right_connected = false,
  .left_battery = 0,
  .right_battery = 0,
  .usb_connected = false
};

/**
 * @brief 상태바 생성 (그래픽 기반)
 */
void apStatusBarCreate(void)
{
  // 상단 상태바 컨테이너 (높이 증가)
  status_bar = lv_obj_create(lv_scr_act());
  lv_obj_set_size(status_bar, 240, 50);
  lv_obj_set_pos(status_bar, 0, 0);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_radius(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 0, 0);
  lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // ============ 왼쪽 키보드 (왼쪽 상단) ============
  left_kb_icon = lv_obj_create(status_bar);
  lv_obj_set_size(left_kb_icon, 70, 45);
  lv_obj_set_pos(left_kb_icon, 2, 2);
  lv_obj_set_style_bg_color(left_kb_icon, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_color(left_kb_icon, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(left_kb_icon, 1, 0);
  lv_obj_set_style_radius(left_kb_icon, 3, 0);
  lv_obj_set_style_pad_all(left_kb_icon, 3, 0);
  lv_obj_clear_flag(left_kb_icon, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // 왼쪽 연결 상태 표시 점
  left_status_dot = lv_obj_create(left_kb_icon);
  lv_obj_set_size(left_status_dot, 8, 8);
  lv_obj_set_pos(left_status_dot, 3, 3);
  lv_obj_set_style_bg_color(left_status_dot, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(left_status_dot, 0, 0);
  lv_obj_set_style_radius(left_status_dot, LV_RADIUS_CIRCLE, 0);
  
  // 왼쪽 배터리 외곽선
  left_bat_bar = lv_obj_create(left_kb_icon);
  lv_obj_set_size(left_bat_bar, 50, 12);
  lv_obj_set_pos(left_bat_bar, 10, 18);
  lv_obj_set_style_bg_color(left_bat_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_color(left_bat_bar, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(left_bat_bar, 2, 0);
  lv_obj_set_style_radius(left_bat_bar, 2, 0);
  lv_obj_set_style_pad_all(left_bat_bar, 2, 0);
  
  // 왼쪽 배터리 채우기
  left_bat_fill = lv_obj_create(left_bat_bar);
  lv_obj_set_size(left_bat_fill, 0, 8);  // 초기 0%
  lv_obj_set_pos(left_bat_fill, 0, 0);
  lv_obj_set_style_bg_color(left_bat_fill, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(left_bat_fill, 0, 0);
  lv_obj_set_style_radius(left_bat_fill, 1, 0);
  
  // 왼쪽 배터리 팁
  lv_obj_t *left_bat_tip = lv_obj_create(left_kb_icon);
  lv_obj_set_size(left_bat_tip, 3, 6);
  lv_obj_set_pos(left_bat_tip, 61, 21);
  lv_obj_set_style_bg_color(left_bat_tip, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(left_bat_tip, 0, 0);
  lv_obj_set_style_radius(left_bat_tip, 1, 0);
  
  // ============ USB 아이콘 (중앙) ============
  usb_icon = lv_obj_create(status_bar);
  lv_obj_set_size(usb_icon, 50, 45);
  lv_obj_align(usb_icon, LV_ALIGN_TOP_MID, 0, 2);
  lv_obj_set_style_bg_color(usb_icon, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_color(usb_icon, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(usb_icon, 1, 0);
  lv_obj_set_style_radius(usb_icon, 3, 0);
  lv_obj_set_style_pad_all(usb_icon, 0, 0);
  lv_obj_clear_flag(usb_icon, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // USB 본체 (아래쪽)
  usb_body = lv_obj_create(usb_icon);
  lv_obj_set_size(usb_body, 24, 16);
  lv_obj_set_pos(usb_body, 13, 22);
  lv_obj_set_style_bg_color(usb_body, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(usb_body, 0, 0);
  lv_obj_set_style_radius(usb_body, 2, 0);
  
  // USB 플러그 (위쪽)
  usb_plug = lv_obj_create(usb_icon);
  lv_obj_set_size(usb_plug, 16, 18);
  lv_obj_set_pos(usb_plug, 17, 7);
  lv_obj_set_style_bg_color(usb_plug, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(usb_plug, 0, 0);
  lv_obj_set_style_radius(usb_plug, 1, 0);
  
  // USB 플러그 내부 (핀 표현)
  lv_obj_t *usb_pin1 = lv_obj_create(usb_plug);
  lv_obj_set_size(usb_pin1, 3, 6);
  lv_obj_set_pos(usb_pin1, 3, 9);
  lv_obj_set_style_bg_color(usb_pin1, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(usb_pin1, 0, 0);
  
  lv_obj_t *usb_pin2 = lv_obj_create(usb_plug);
  lv_obj_set_size(usb_pin2, 3, 6);
  lv_obj_set_pos(usb_pin2, 10, 9);
  lv_obj_set_style_bg_color(usb_pin2, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(usb_pin2, 0, 0);
  
  // ============ 오른쪽 키보드 (오른쪽 상단) ============
  right_kb_icon = lv_obj_create(status_bar);
  lv_obj_set_size(right_kb_icon, 70, 45);
  lv_obj_align(right_kb_icon, LV_ALIGN_TOP_RIGHT, -2, 2);
  lv_obj_set_style_bg_color(right_kb_icon, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_color(right_kb_icon, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(right_kb_icon, 1, 0);
  lv_obj_set_style_radius(right_kb_icon, 3, 0);
  lv_obj_set_style_pad_all(right_kb_icon, 3, 0);
  lv_obj_clear_flag(right_kb_icon, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // 오른쪽 연결 상태 표시 점
  right_status_dot = lv_obj_create(right_kb_icon);
  lv_obj_set_size(right_status_dot, 8, 8);
  lv_obj_align(right_status_dot, LV_ALIGN_TOP_RIGHT, -3, 3);
  lv_obj_set_style_bg_color(right_status_dot, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(right_status_dot, 0, 0);
  lv_obj_set_style_radius(right_status_dot, LV_RADIUS_CIRCLE, 0);
  
  // 오른쪽 배터리 외곽선
  right_bat_bar = lv_obj_create(right_kb_icon);
  lv_obj_set_size(right_bat_bar, 50, 12);
  lv_obj_set_pos(right_bat_bar, 7, 18);
  lv_obj_set_style_bg_color(right_bat_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_color(right_bat_bar, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(right_bat_bar, 2, 0);
  lv_obj_set_style_radius(right_bat_bar, 2, 0);
  lv_obj_set_style_pad_all(right_bat_bar, 2, 0);
  
  // 오른쪽 배터리 채우기
  right_bat_fill = lv_obj_create(right_bat_bar);
  lv_obj_set_size(right_bat_fill, 0, 8);  // 초기 0%
  lv_obj_set_pos(right_bat_fill, 0, 0);
  lv_obj_set_style_bg_color(right_bat_fill, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_width(right_bat_fill, 0, 0);
  lv_obj_set_style_radius(right_bat_fill, 1, 0);
  
  // 오른쪽 배터리 팁
  lv_obj_t *right_bat_tip = lv_obj_create(right_kb_icon);
  lv_obj_set_size(right_bat_tip, 3, 6);
  lv_obj_set_pos(right_bat_tip, 58, 21);
  lv_obj_set_style_bg_color(right_bat_tip, lv_color_hex(0x888888), 0);
  lv_obj_set_style_border_width(right_bat_tip, 0, 0);
  lv_obj_set_style_radius(right_bat_tip, 1, 0);
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
    // 연결 상태 점: 녹색
    lv_obj_set_style_bg_color(left_status_dot, lv_color_hex(0x00FF00), 0);
    
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
    
    // 배터리 바 너비 조정
    int16_t bat_width = (battery * 46) / 100;
    lv_obj_set_width(left_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨간색 점
    lv_obj_set_style_bg_color(left_status_dot, lv_color_hex(0xFF0000), 0);
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
    // 연결 상태 점: 녹색
    lv_obj_set_style_bg_color(right_status_dot, lv_color_hex(0x00FF00), 0);
    
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
    
    // 배터리 바 너비 조정
    int16_t bat_width = (battery * 46) / 100;
    lv_obj_set_width(right_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨간색 점
    lv_obj_set_style_bg_color(right_status_dot, lv_color_hex(0xFF0000), 0);
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
    // USB 연결됨: 녹색
    lv_obj_set_style_bg_color(usb_body, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_color(usb_plug, lv_color_hex(0x00FF00), 0);
  }
  else
  {
    // USB 연결 끊김: 빨간색
    lv_obj_set_style_bg_color(usb_body, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_color(usb_plug, lv_color_hex(0xFF0000), 0);
  }
}
