#include "ap_lvgl.h"
#include "ap_status_bar.h"
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

// 전역 변수
static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);
static struct k_thread lvgl_thread_data;
static k_tid_t lvgl_thread_id = NULL;

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

// 타이머
static lv_timer_t *update_timer = NULL;

// 스레드 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);
static void create_tamago_screen(void);
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
  apStatusBarCreate();
  
  // 다마고치 화면 생성
  create_tamago_screen();
  
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
