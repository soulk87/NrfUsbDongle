#include "ap.h"
// main.c
#include <zephyr/kernel.h>
#include "ap/my_key_protocol.h"
#define CLI_THREAD_STACK_SIZE 4096
#define CLI_THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(cli_thread_stack, CLI_THREAD_STACK_SIZE);
// 스레드 데이터 구조체
static struct k_thread cli_thread_data;
// 스레드 ID
static k_tid_t cli_thread_id = NULL;
// 스레드 함수 프로토타입

static void cli_thread_func(void *arg1, void *arg2, void *arg3);

void apInit(void)
{
  // 스레드 생성
  cli_thread_id = k_thread_create(&cli_thread_data, cli_thread_stack,
                                  K_THREAD_STACK_SIZEOF(cli_thread_stack),
                                  cli_thread_func, NULL, NULL, NULL,
                                  CLI_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void apMain(void)
{
  delay(10);

  while (1)
  {
    // key scan

    delay(5);

    // trackball read
    int32_t x = 0, y = 0;
    if (pmw3610_motion_read(&x, &y))
    {
      key_protocol_send_trackball_data(KEY_BOARD_ID, (int16_t)x, (int16_t)y);
    }
    delay(5);
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