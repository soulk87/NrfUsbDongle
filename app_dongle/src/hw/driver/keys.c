#include "keys.h"

#ifdef _USE_HW_KEYS
#include "qbuffer.h"
#include "cli.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

// 스레드 관련 변수 추가
#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(key_thread_stack, THREAD_STACK_SIZE);
static struct k_thread key_thread_data;

#define KEYS_ROWS MATRIX_ROWS
#define KEYS_COLS MATRIX_COLS

#define lock()       // xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock()     // xSemaphoreGive(mutex_lock);
#define lockGpio()   // xSemaphoreTake(mutex_gpio, portMAX_DELAY);
#define unLockGpio() // xSemaphoreGive(mutex_gpio);

#ifdef _USE_CLI_HW_KEYS
static void cliCmd(cli_args_t *args);
#endif

static void keysThread(void *arg1, void *arg2, void *arg3);
static bool keysInitGpio(void);

static uint8_t cols_buf[KEYS_COLS];


/* 배열로 직접 선언 */
static const struct gpio_dt_spec rows_gpio_tbl[KEYS_ROWS] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(row0), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(row1), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(row2), gpios),
};

static const struct gpio_dt_spec cols_gpio_tbl[KEYS_COLS] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(col0), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(col1), gpios),
};


static bool is_ready = false;

bool keysInit(void)
{

  keysInitGpio();
  // TODO: create semaphore
  // TODO: create thread


  k_msleep(1000);
  k_thread_create(&key_thread_data, key_thread_stack,
                  K_THREAD_STACK_SIZEOF(key_thread_stack),
                  keysThread,
                  NULL, NULL, NULL,
                  THREAD_PRIORITY, 0, K_NO_WAIT);

#ifdef _USE_CLI_HW_KEYS
  cliAdd("keys", cliCmd);
#endif

  return true;
}

bool keysInitGpio(void)
{
  lockGpio();

  for (int i = 0; i < ARRAY_SIZE(rows_gpio_tbl); i++) {
      gpio_pin_configure_dt(&rows_gpio_tbl[i], GPIO_INPUT | GPIO_PULL_UP);
  }

  for (int j = 0; j < ARRAY_SIZE(cols_gpio_tbl); j++) {
      gpio_pin_configure_dt(&cols_gpio_tbl[j], GPIO_OUTPUT_ACTIVE);
  }

  unLockGpio();

  return true;
}

bool keysEnterSleep(void)
{
  // esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  // gpio_config_t config = {
  //   .pin_bit_mask = BIT64(GPIO_NUM_18),
  //   .mode         = GPIO_MODE_INPUT,
  //   .pull_down_en = true,
  //   .pull_up_en   = false,
  //   .intr_type    = GPIO_INTR_DISABLE};

  // for (int i=0; i<KEYS_COLS; i++)
  // {
  //   config.pin_bit_mask = BIT64(cols_gpio_tbl[i]);
  //   gpio_config(&config);
  //   gpio_wakeup_enable(cols_gpio_tbl[i], GPIO_INTR_HIGH_LEVEL);
  // }

  // esp_sleep_enable_gpio_wakeup();
  return true;
}

bool keysExitSleep(void)
{
  // esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  // keysInitGpio();
  return true;
}

bool keysIsBusy(void)
{
  return false;
}

bool keysIsReady(void)
{
  bool ret = is_ready;

  is_ready = false;

  return ret;
}

bool keysUpdate(void)
{
  return true;
}

bool keysReadBuf(uint8_t *p_data, uint32_t length)
{
  lock();
  memcpy(p_data, cols_buf, length);
  unLock();
  return true;
}

bool keysGetPressed(uint16_t row, uint16_t col)
{
  bool ret = false;
  uint8_t row_bit;

  row_bit = cols_buf[col];

  if (row_bit & (1 << row))
  {
    ret = true;
  }

  return ret;
}

void keysScan(void)
{
  uint8_t scan_buf[KEYS_COLS];

  memset(scan_buf, 0, sizeof(scan_buf));

  // lockGpio();
  for (int cols_i = 0; cols_i < KEYS_COLS; cols_i++)
  {
    gpio_pin_set_dt(&cols_gpio_tbl[cols_i], 0);
    k_usleep(10);
    for (int rows_i = 0; rows_i < KEYS_ROWS; rows_i++)
    {
      if (gpio_pin_get_dt(&rows_gpio_tbl[rows_i]) == 0)
      {
        scan_buf[cols_i] |= (1 << rows_i);
      }
    }
    gpio_pin_set_dt(&cols_gpio_tbl[cols_i], 1);
  }
  // unLockGpio();

  lock();
  memcpy(cols_buf, scan_buf, sizeof(scan_buf));
  unLock();

  is_ready = true;
}

static void keysThread(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1)
  {
    keysScan();
    delay(1);
  }
}

#ifdef _USE_CLI_HW_KEYS
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliShowCursor(false);

    while (cliKeepLoop())
    {
      keysUpdate();
      delay(10);

      cliPrintf("     ");
      for (int cols = 0; cols < MATRIX_COLS; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows = 0; rows < MATRIX_ROWS; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols = 0; cols < MATRIX_COLS; cols++)
        {
          if (keysGetPressed(rows, cols))
            cliPrintf("O  ");
          else
            cliPrintf("_  ");
        }
        cliPrintf("\n");
      }
      cliMoveUp(MATRIX_ROWS + 1);
    }
    cliMoveDown(MATRIX_ROWS + 1);

    cliShowCursor(true);
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "rowcol"))
  {
    uint16_t row;
    uint16_t col;

    row = (uint16_t)args->getData(1);
    col = (uint16_t)args->getData(2);

    // change key state
    cols_buf[col] ^= (1 << row);
    cliPrintf("keys row %d col %d state change\n", row, col);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("keys info\n");
    cliPrintf("keys rowcol [row] [col]\n");
  }
}
#endif

#endif