#include "ap_lvgl.h"
#include "ap.h"
#include "lvgl/lvgl.h"

#define LVGL_THREAD_STACK_SIZE 4096
#define LVGL_THREAD_PRIORITY 6

static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);

// 스레드 데이터 구조체
static struct k_thread lvgl_thread_data;

// 스레드 ID
static k_tid_t lvgl_thread_id = NULL;

// 스레드 함수 프로토타입
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);

/**
 * @brief LVGL 초기화 및 Hello World 위젯 생성
 */
static void lvgl_hello_world_init(void)
{
  lv_init();
  
  // 메인 스크린 생성
  lv_obj_t *scr = lv_scr_act();
  
  // Hello World 라벨 생성
  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "Hello World!\nLVGL Test");
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  
  // 라벨 스타일 설정
  lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
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
  lvgl_hello_world_init();
  
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
