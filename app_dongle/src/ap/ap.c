#include "ap.h"
// main.c
#include <zephyr/kernel.h>
#include "qmk/qmk.h"
#define CLI_THREAD_STACK_SIZE 4096
#define CLI_THREAD_PRIORITY 5

#include "lvgl/lvgl.h"

#define LVGL_THREAD_STACK_SIZE 4096
#define LVGL_THREAD_PRIORITY 6

static K_THREAD_STACK_DEFINE(cli_thread_stack, CLI_THREAD_STACK_SIZE);
static K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);

// 스레드 데이터 구조체
static struct k_thread cli_thread_data;
static struct k_thread lvgl_thread_data;

// 스레드 ID
static k_tid_t cli_thread_id = NULL;
static k_tid_t lvgl_thread_id = NULL;

// 스레드 함수 프로토타입
static void cli_thread_func(void *arg1, void *arg2, void *arg3);
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3);

// LVGL 초기화 및 Hello World 위젯 생성
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

void apInit(void)
{ 
  // LVGL 스레드 생성
  lvgl_thread_id = k_thread_create(&lvgl_thread_data, lvgl_thread_stack,
                                   K_THREAD_STACK_SIZEOF(lvgl_thread_stack),
                                   lvgl_thread_func, NULL, NULL, NULL,
                                   LVGL_THREAD_PRIORITY, 0, K_NO_WAIT);
  
  // 스레드 생성
  cli_thread_id = k_thread_create(&cli_thread_data, cli_thread_stack,
                                  K_THREAD_STACK_SIZEOF(cli_thread_stack),
                                  cli_thread_func, NULL, NULL, NULL,
                                  CLI_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void apMain(void)
{
  // uint32_t pre_time;
  uint8_t index = 0;
  qmkInit();
  delay(100);
  // pre_time = millis();
  while (1)
  {
    // if (millis() - pre_time >= 2000)
    // {
    //   pre_time = millis();

    //   index++;
    // }
    qmkUpdate();
    delay(1);
  }
}

// LVGL 스레드 함수
static void lvgl_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);
    // LVGL 초기화
  lvgl_hello_world_init();

  // lv_timer_handler();
  
  while (1)
  {
    lv_timer_handler();
    delay(5); // 5ms 간격으로 LVGL 타이머 처리
  }
}

// 스레드 함수
static void cli_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);
  while (1)
  {
    cliOpen(HW_UART_CH_CLI, 115200);
    cliMain();
    delay(2);
  }
}