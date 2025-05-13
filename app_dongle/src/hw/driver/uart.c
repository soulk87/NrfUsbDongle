#include "uart.h"

#ifdef _USE_HW_UART
// #include "driver/uart.h"
#ifdef _USE_HW_CDC
#include "cdc.h"
#endif

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <stdarg.h>
#include "qbuffer.h"

LOG_MODULE_REGISTER(uart_driver, CONFIG_UART_LOG_LEVEL);

#define UART_RX_BUF_SIZE 128
#define RECEIVE_TIMEOUT 100
typedef struct
{
  bool is_open;
  uint32_t port;
  uint32_t baud;
  qbuffer_t queue;
  uint8_t wr_buf[UART_RX_BUF_SIZE];
  const struct device *dev;
} uart_tbl_t;

// 스레드 관련 변수 추가
#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(usbcdc_thread_stack, THREAD_STACK_SIZE);
static struct k_thread usbcdc_thread_data;
static k_tid_t usbcdc_thread_id = NULL;
static struct k_sem usbcdc_sem;

static bool is_thread_running = false;
static uint8_t thread_ch;
static uint32_t thread_baud;

static uart_tbl_t uart_tbl[UART_MAX_CH] =
    {
        {.is_open = false,
         .port = _DEF_UART1,
         .baud = 115200,
         .dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0))}};

static void usbcdc_interrupt_handler(const struct device *dev, void *user_data)
{
  ARG_UNUSED(user_data);

  while (uart_irq_update(dev) && uart_irq_is_pending(dev))
  {
    if (uart_irq_rx_ready(dev))
    {
      int recv_len;
      uint8_t buffer[UART_RX_BUF_SIZE];

      recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
      if (recv_len < 0)
      {
        LOG_ERR("Failed to read UART FIFO");
        recv_len = 0;
      }

      qbufferWrite(&uart_tbl[_DEF_UART1].queue, buffer, recv_len);
    }
  }
}

bool uartInit(void)
{

  for (int i = 0; i < UART_MAX_CH; i++)
  {
    uart_tbl[i].is_open = false;
  }

  return true;
}

// 원래의 usbcdc_Init 함수를 내부용으로 변경
static bool usbcdc_Init_internal(uint8_t ch, uint32_t baud)
{
  int ret;

  // Initialize qbuffer for UART RX
  qbufferCreate(&uart_tbl[ch].queue, uart_tbl[ch].wr_buf, UART_RX_BUF_SIZE);

  // Check if CDC ACM device is ready
  if (!device_is_ready(uart_tbl[ch].dev))
  {
    LOG_ERR("CDC ACM device not ready");
    return false;
  }

  while (true)
  {
    uint32_t dtr = 0U;

    uart_line_ctrl_get(uart_tbl[ch].dev, UART_LINE_CTRL_DTR, &dtr);
    if (dtr)
    {
      break;
    }
    else
    {
      /* Give CPU resources to low priority threads. */
      k_sleep(K_MSEC(100));
    }
  }
  LOG_INF("DTR set");

  /* They are optional, we use them to test the interrupt endpoint */
  ret = uart_line_ctrl_set(uart_tbl[ch].dev, UART_LINE_CTRL_DCD, 1);
  if (ret)
  {
    LOG_WRN("Failed to set DCD, ret code %d", ret);
    return false;
  }

  ret = uart_line_ctrl_set(uart_tbl[ch].dev, UART_LINE_CTRL_DSR, 1);
  if (ret)
  {
    LOG_WRN("Failed to set DSR, ret code %d", ret);
    return false;
  }
  /* Wait 100ms for the host to do all settings */
  k_msleep(100);

  ret = uart_line_ctrl_set(uart_tbl[ch].dev, UART_LINE_CTRL_BAUD_RATE, baud);
  if (ret)
  {
    LOG_ERR("Failed to set baudrate to %d", baud);
    return false;
  }
  uart_tbl[ch].baud = baud;
  /* Wait 100ms for the host to do all settings */
  k_msleep(100);

  uart_irq_callback_set(uart_tbl[ch].dev, usbcdc_interrupt_handler);

  /* Enable rx interrupts */
  uart_irq_rx_enable(uart_tbl[ch].dev);

  uart_tbl[ch].is_open = true;
  LOG_INF("USB CDC initialized on channel %d with baudrate %d", ch, baud);

  return true;
}

// 스레드 함수
static void usbcdc_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);
  
  uint8_t ch = thread_ch;
  uint32_t baud = thread_baud;
  
  LOG_INF("USB CDC thread started for channel %d with baud %d", ch, baud);
  
  // 초기화 수행
  (void)usbcdc_Init_internal(ch, baud);
  
  // 초기화 완료 알림
  k_sem_give(&usbcdc_sem);
  
  // 스레드 종료
  is_thread_running = false;
}

// 외부에서 호출하는 usbcdc_Init 함수 수정
bool usbcdc_Init(uint8_t ch, uint32_t baud)
{
  if (is_thread_running)
  {
    LOG_WRN("usbcdc_Init is already running in thread");
    return false;
  }
  
  // 세마포어 초기화 (처음 호출시에만)
  static bool sem_initialized = false;
  if (!sem_initialized)
  {
    k_sem_init(&usbcdc_sem, 0, 1);
    sem_initialized = true;
  }
  
  // 스레드 파라미터 설정
  thread_ch = ch;
  thread_baud = baud;
  is_thread_running = true;
  
  // 스레드 생성 및 실행
  usbcdc_thread_id = k_thread_create(&usbcdc_thread_data, usbcdc_thread_stack,
                                     K_THREAD_STACK_SIZEOF(usbcdc_thread_stack),
                                     usbcdc_thread_func,
                                     NULL, NULL, NULL,
                                     THREAD_PRIORITY, 0, K_NO_WAIT);
  if (usbcdc_thread_id == NULL)
  {
    LOG_ERR("Failed to create USB CDC thread");
    is_thread_running = false;
    return false;
  }
  
  k_thread_name_set(usbcdc_thread_id, "usbcdc_thread");
  
  return true;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  int ret;

  if (ch >= UART_MAX_CH)
  {
    return false;
  }

  if (uart_tbl[ch].is_open == true && uart_tbl[ch].baud == baud)
  {
    return true;
  }

  if (ch == _DEF_UART1 && device_is_ready(uart_tbl[ch].dev))
  {

    if (uart_tbl[ch].is_open == false)
    {
      // 스레드가 이미 실행 중인지 확인
      if (is_thread_running)
      {
        LOG_DBG("uartOpen: USB CDC initialization is already in progress");
        return false;
      }

      if (usbcdc_Init(ch, baud) == false)
      {
        LOG_ERR("Failed to initialize UART");
        return false;
      }

      // 스레드로 초기화 중임을 알림
      LOG_INF("USB CDC initialization started in thread");
      return false;
    }

    // Set baudrate if needed
    if (baud != uart_tbl[ch].baud)
    {
      ret = uart_line_ctrl_set(uart_tbl[ch].dev, UART_LINE_CTRL_BAUD_RATE, baud);
      if (ret)
      {
        LOG_ERR("Failed to set baudrate to %d", baud);
        return false;
      }
      uart_tbl[ch].baud = baud;

      uart_tbl[ch].is_open = true;
    }

    return true;
  }

  return false;
}

bool uartIsOpen(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
  {
    return false;
  }

  return uart_tbl[ch].is_open;
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
  {
    return false;
  }

  uart_tbl[ch].is_open = false;
  return true;
}

uint32_t uartAvailable(uint8_t ch)
{
  if (ch >= UART_MAX_CH || !uart_tbl[ch].is_open)
  {
    return 0;
  }

  if (ch == _DEF_UART1)
  {
    return qbufferAvailable(&uart_tbl[ch].queue);
  }

  return 0;
}

bool uartFlush(uint8_t ch)
{
  if (ch >= UART_MAX_CH || !uart_tbl[ch].is_open)
  {
    return false;
  }

  if (ch == _DEF_UART1)
  {
    qbufferFlush(&uart_tbl[ch].queue);
    return true;
  }

  return false;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;

  if (ch >= UART_MAX_CH || !uart_tbl[ch].is_open)
  {
    return 0;
  }

  if (ch == _DEF_UART1)
  {
    if (qbufferAvailable(&uart_tbl[ch].queue) > 0)
    {
      qbufferRead(&uart_tbl[ch].queue, &ret, 1);
    }
  }

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{

  if (ch >= UART_MAX_CH || !uart_tbl[ch].is_open || p_data == NULL)
  {
    return 0;
  }

  if (ch == _DEF_UART1 && device_is_ready(uart_tbl[ch].dev))
  {
    uart_fifo_fill(uart_tbl[ch].dev, p_data, length);
    return length;
  }

  return 0;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);
  va_end(args);

  if (len > 0)
  {
    ret = uartWrite(ch, (uint8_t *)buf, len);
    return ret;
  }
  else
  {
    return 0;
  }
}

uint32_t uartGetBaud(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
  {
    return 0;
  }

  return uart_tbl[ch].baud;
}
#endif
