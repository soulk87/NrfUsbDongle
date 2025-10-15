#include "ap_lvgl.h"
#include "ap.h"
#include "lvgl/lvgl.h"
#include <stdlib.h>

#define LVGL_THREAD_STACK_SIZE 4096
#define LVGL_THREAD_PRIORITY 6

// 다마고치 상태 정의
typedef enum {
  TAMAGO_STATE_EGG,           // 계란
  TAMAGO_STATE_CRACKED,       // 금이 간 계란
  TAMAGO_STATE_HATCHED,       // 부화된 캐릭터
} tamago_state_t;

// 캐릭터 타입 (5가지)
typedef enum {
  CHAR_TYPE_CAT = 0,
  CHAR_TYPE_DOG,
  CHAR_TYPE_BIRD,
  CHAR_TYPE_RABBIT,
  CHAR_TYPE_BEAR,
  CHAR_TYPE_MAX
} character_type_t;

// 상태바 데이터 구조체
typedef struct {
  bool left_connected;
  bool right_connected;
  uint8_t left_battery;
  uint8_t right_battery;
  bool usb_connected;
} status_bar_data_t;

// 전역 변수
static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);
static struct k_thread lvgl_thread_data;
static k_tid_t lvgl_thread_id = NULL;

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

// UI 객체들 - 메인 화면
static lv_obj_t *tamago_label;
static lv_obj_t *main_screen;

// 다마고치 상태 변수
static tamago_state_t current_state = TAMAGO_STATE_EGG;
static character_type_t current_char = CHAR_TYPE_CAT;
static uint32_t last_key_time = 0;
static uint32_t last_state_change_time = 0;
static bool is_dancing = false;
static uint32_t dance_start_time = 0;

// 상태바 데이터
static status_bar_data_t status_data = {
  .left_connected = false,
  .right_connected = false,
  .left_battery = 0,
  .right_battery = 0,
  .usb_connected = false
};

// 타이머
static lv_timer_t *update_timer = NULL;

// 스레드 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);
static void create_status_bar(void);
static void create_tamago_screen(void);
static void update_status_bar(void);
static void update_tamago(lv_timer_t *timer);
static const char* get_character_emoji(character_type_t type, bool dancing);
static void change_character(void);

// 스레드 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);
static void create_status_bar(void);
static void create_tamago_screen(void);
static void update_status_bar(void);
static void update_tamago(lv_timer_t *timer);
static const char* get_character_emoji(character_type_t type, bool dancing);
static void change_character(void);

/**
 * @brief 캐릭터 이모지 반환 (텍스트 기반)
 */
static const char* get_character_emoji(character_type_t type, bool dancing)
{
  static const char* characters[CHAR_TYPE_MAX][2] = {
    // [캐릭터][0: 일반, 1: 춤]
    {"  /\\_/\\\n ( o.o )\n  > ^ <", "  /\\_/\\\n ( ^.^ )\n  \\ | /"},  // CAT
    {" / \\\n(o o)\n \\_/", " \\o/\n(^ ^)\n / \\"},  // DOG
    {"  _\n (o>\n //\\\n \\/", "  _\n <o)\n //\\\n <\\"},  // BIRD
    {" /\\ /\\\n( o o )\n(  >  )", " /\\ /\\\n( ^ ^ )\n(  v  )"},  // RABBIT
    {" ('-')\n(> <)\n(_|_)", " \\o/\n(^ ^)\n(_|_)"}   // BEAR
  };
  
  return characters[type][dancing ? 1 : 0];
}

/**
 * @brief 랜덤 캐릭터 변경
 */
static void change_character(void)
{
  current_char = (character_type_t)(rand() % CHAR_TYPE_MAX);
}

/**
 * @brief 상태바 생성 (그래픽 기반)
 */
static void create_status_bar(void)
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
 * @brief 다마고치 메인 화면 생성
 */
static void create_tamago_screen(void)
{
  // 메인 화면 컨테이너 (상태바 높이 증가로 인한 조정)
  main_screen = lv_obj_create(lv_scr_act());
  lv_obj_set_size(main_screen, 240, 190);
  lv_obj_set_pos(main_screen, 0, 50);  // 상태바 50px
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(main_screen, 0, 0);
  lv_obj_set_style_radius(main_screen, 0, 0);
  lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);  // 스크롤 비활성화
  
  // 다마고치 라벨 (중앙)
  tamago_label = lv_label_create(main_screen);
  lv_label_set_text(tamago_label, "\n  ___\n /   \\\n|  o  |\n \\___/\n EGG");
  lv_obj_set_style_text_align(tamago_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(tamago_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(tamago_label, &lv_font_montserrat_14, 0);
  lv_obj_align(tamago_label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief 상태바 업데이트 (내부용 - 그래픽 기반)
 */
static void update_status_bar(void)
{
  // 왼쪽 키보드 상태
  if (status_data.left_connected)
  {
    // 연결 상태 점: 녹색
    lv_obj_set_style_bg_color(left_status_dot, lv_color_hex(0x00FF00), 0);
    
    // 배터리 색상 (0-20%: 빨강, 21-50%: 노랑, 51-100%: 녹색)
    uint32_t bat_color;
    if (status_data.left_battery <= 20) {
      bat_color = 0xFF0000;  // 빨강
    } else if (status_data.left_battery <= 50) {
      bat_color = 0xFFAA00;  // 노랑
    } else {
      bat_color = 0x00FF00;  // 녹색
    }
    lv_obj_set_style_bg_color(left_bat_fill, lv_color_hex(bat_color), 0);
    
    // 배터리 바 너비 조정 (최대 46px)
    int16_t bat_width = (status_data.left_battery * 46) / 100;
    lv_obj_set_width(left_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨간색 점
    lv_obj_set_style_bg_color(left_status_dot, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(left_bat_fill, 0);
  }
  
  // 오른쪽 키보드 상태
  if (status_data.right_connected)
  {
    // 연결 상태 점: 녹색
    lv_obj_set_style_bg_color(right_status_dot, lv_color_hex(0x00FF00), 0);
    
    // 배터리 색상
    uint32_t bat_color;
    if (status_data.right_battery <= 20) {
      bat_color = 0xFF0000;  // 빨강
    } else if (status_data.right_battery <= 50) {
      bat_color = 0xFFAA00;  // 노랑
    } else {
      bat_color = 0x00FF00;  // 녹색
    }
    lv_obj_set_style_bg_color(right_bat_fill, lv_color_hex(bat_color), 0);
    
    // 배터리 바 너비 조정
    int16_t bat_width = (status_data.right_battery * 46) / 100;
    lv_obj_set_width(right_bat_fill, bat_width);
  }
  else
  {
    // 연결 끊김: 빨간색 점
    lv_obj_set_style_bg_color(right_status_dot, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(right_bat_fill, 0);
  }
  
  // USB 상태
  if (status_data.usb_connected)
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

/**
 * @brief 다마고치 상태 업데이트 (타이머 콜백)
 */
static void update_tamago(lv_timer_t *timer)
{
  ARG_UNUSED(timer);
  uint32_t current_time = k_uptime_get_32();
  
  // 부화 상태일 때
  if (current_state == TAMAGO_STATE_HATCHED)
  {
    // 춤 애니메이션 처리
    if (is_dancing)
    {
      if (current_time - dance_start_time >= 500)  // 0.5초 동안 춤
      {
        is_dancing = false;
        lv_label_set_text(tamago_label, get_character_emoji(current_char, false));
      }
    }
    
    // 30초간 키 입력 없으면 다시 계란으로
    if (current_time - last_key_time >= 30000)
    {
      current_state = TAMAGO_STATE_EGG;
      lv_label_set_text(tamago_label, "\n  ___\n /   \\\n|  o  |\n \\___/\n EGG");
      last_state_change_time = current_time;
    }
  }
  
  // ========== 테스트: 키보드 상태 자동 업데이트 (순서대로) ==========
  static uint32_t test_update_counter = 0;
  static uint8_t test_step = 0;
  test_update_counter++;
  
  // 2초마다 상태 변경 (100ms * 20 = 2000ms)
  if (test_update_counter % 20 == 0)
  {
    test_step++;
    
    switch (test_step)
    {
      case 1:
        // 왼쪽 키보드 연결 안됨
        apLvglUpdateLeftKeyboard(false, 0);
        apLvglUpdateRightKeyboard(false, 0);
        apLvglUpdateUSB(false);
        break;
        
      case 2:
        // 왼쪽 키보드 연결됨, 배터리 10% (빨강)
        apLvglUpdateLeftKeyboard(true, 10);
        break;
        
      case 3:
        // 왼쪽 키보드 배터리 35% (노랑)
        apLvglUpdateLeftKeyboard(true, 35);
        break;
        
      case 4:
        // 왼쪽 키보드 배터리 80% (녹색)
        apLvglUpdateLeftKeyboard(true, 80);
        break;
        
      case 5:
        // 오른쪽 키보드 연결됨, 배터리 15% (빨강)
        apLvglUpdateRightKeyboard(true, 15);
        break;
        
      case 6:
        // 오른쪽 키보드 배터리 45% (노랑)
        apLvglUpdateRightKeyboard(true, 45);
        break;
        
      case 7:
        // 오른쪽 키보드 배터리 95% (녹색)
        apLvglUpdateRightKeyboard(true, 95);
        break;
        
      case 8:
        // USB 연결됨
        apLvglUpdateUSB(true);
        break;
        
      case 9:
        // 모두 연결됨, 왼쪽 50%, 오른쪽 60%
        apLvglUpdateLeftKeyboard(true, 50);
        apLvglUpdateRightKeyboard(true, 60);
        apLvglUpdateUSB(true);
        break;
        
      case 10:
        // 왼쪽 배터리 감소
        apLvglUpdateLeftKeyboard(true, 30);
        break;
        
      case 11:
        // 오른쪽 배터리 감소
        apLvglUpdateRightKeyboard(true, 25);
        break;
        
      case 12:
        // USB 연결 끊김
        apLvglUpdateUSB(false);
        break;
        
      default:
        // 처음으로 돌아감
        test_step = 0;
        break;
    }
  }
}

/**
 * @brief LVGL 초기화 및 다마고치 화면 생성
 */
static void lvgl_tamago_init(void)
{
  lv_init();
  
  // 메인 스크린 배경 설정
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  
  // 상태바 생성
  create_status_bar();
  
  // 다마고치 화면 생성
  create_tamago_screen();
  
  // 상태바 초기 업데이트
  update_status_bar();
  
  // 타이머 생성 (100ms마다 업데이트)
  update_timer = lv_timer_create(update_tamago, 100, NULL);
  
  // 랜덤 시드 초기화
  srand(k_uptime_get_32());
  
  // 초기 시간 설정
  last_state_change_time = k_uptime_get_32();
  last_key_time = k_uptime_get_32();
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
  lvgl_tamago_init();
  
  while (1)
  {
    lv_timer_handler();
    delay(5); // 5ms 간격으로 LVGL 타이머 처리
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
 * @brief 키 입력 알림 (외부에서 호출)
 */
void apLvglNotifyKeyPress(void)
{
  uint32_t current_time = k_uptime_get_32();
  last_key_time = current_time;
  
  // 계란 상태일 때 키 입력이 오면 부화 시작
  if (current_state == TAMAGO_STATE_EGG)
  {
    current_state = TAMAGO_STATE_CRACKED;
    lv_label_set_text(tamago_label, "\n  ___\n / | \\\n| |o| |\n \\___/\nCRACK!");
    last_state_change_time = current_time;
  }
  // 부화된 상태일 때 키 입력이 오면 춤추기
  else if (current_state == TAMAGO_STATE_HATCHED && !is_dancing)
  {
    is_dancing = true;
    dance_start_time = current_time;
    lv_label_set_text(tamago_label, get_character_emoji(current_char, true));
  }
}

/**
 * @brief 왼쪽 키보드 연결 상태 업데이트
 */
void apLvglUpdateLeftKeyboard(bool connected, uint8_t battery)
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
void apLvglUpdateRightKeyboard(bool connected, uint8_t battery)
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
void apLvglUpdateUSB(bool connected)
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
