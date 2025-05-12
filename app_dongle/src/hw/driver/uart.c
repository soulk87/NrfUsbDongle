#include "uart.h"

#ifdef _USE_HW_UART
// #include "driver/uart.h"
#ifdef _USE_HW_CDC
#include "cdc.h"
#endif

/*

#define UART_RX_Q_BUF_LEN       512



typedef struct
{
  bool is_open;

  uart_port_t   port;
  uint32_t      baud;
  uart_config_t config;
  QueueHandle_t queue;

  uint8_t       wr_buf[UART_RX_Q_BUF_LEN];
} uart_tbl_t;


static uart_tbl_t uart_tbl[UART_MAX_CH];


bool uartInit(void)
{
  for (int i=0; i<UART_MAX_CH; i++)
  {
    uart_tbl[i].is_open = false;
  }

  return true;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  bool ret = false;


  if (uartIsOpen(ch) == true && uartGetBaud(ch) == baud)
  {
    return true;
  }

  switch(ch)
  {
    case _DEF_UART1:
      uart_tbl[ch].port = UART_NUM_0;
      uart_tbl[ch].baud = baud;
      uart_tbl[ch].config.baud_rate = baud;
      uart_tbl[ch].config.data_bits = UART_DATA_8_BITS;
      uart_tbl[ch].config.parity    = UART_PARITY_DISABLE;
      uart_tbl[ch].config.stop_bits = UART_STOP_BITS_1;
      uart_tbl[ch].config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
      uart_tbl[ch].config.source_clk = UART_SCLK_APB;


      uart_driver_install(UART_NUM_0, UART_RX_Q_BUF_LEN*2, UART_RX_Q_BUF_LEN*2, 0, NULL, 0);
      uart_param_config(UART_NUM_0, &uart_tbl[ch].config);

      //Set UART pins (using UART0 default pins ie no changes.)
      uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);


      uart_tbl[ch].is_open = true;
      ret = true;
      break;

    case _DEF_UART2:
      uart_tbl[ch].is_open = true;
      uart_tbl[ch].baud = baud;
      ret = true;
      break;
  }

  return ret;
}

bool uartIsOpen(uint8_t ch)
{
  return uart_tbl[ch].is_open;
}

bool uartClose(uint8_t ch)
{
  uart_tbl[ch].is_open = false;
  return true;
}

uint32_t uartAvailable(uint8_t ch)
{
  uint32_t ret = 0;
  size_t len;


  if (uart_tbl[ch].is_open != true) return 0;

  switch(ch)
  {
     case _DEF_UART1:
      uart_get_buffered_data_len(uart_tbl[ch].port, &len);
      ret = len;
      break;

    case _DEF_UART2:
      #ifdef _USE_HW_CDC
      ret = cdcAvailable();
      #endif
      break;
  }

  return ret;
}

bool uartFlush(uint8_t ch)
{
  uint32_t pre_time;

  pre_time = millis();
  while(uartAvailable(ch))
  {
    if (millis()-pre_time >= 10)
    {
      break;
    }
    uartRead(ch);
  }

  switch(ch)
  {
     case _DEF_UART1:
      uart_flush(uart_tbl[ch].port);
      break;
  }

  return true;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;

  switch(ch)
  {
    case _DEF_UART1:
      uart_read_bytes(uart_tbl[ch].port, &ret, 1, 10);
      break;

    case _DEF_UART2:
      #ifdef _USE_HW_CDC
      ret = cdcRead();
      #endif
      break;
  }

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;

  if (uart_tbl[ch].is_open != true) return 0;


  switch(ch)
  {
    case _DEF_UART1:
      ret = uart_write_bytes(uart_tbl[ch].port, (const char*)p_data, (size_t)length);
      break;

    case _DEF_UART2:
      #ifdef _USE_HW_CDC
      ret = cdcWrite(p_data, length);
      #endif
      break;
  }

  return ret;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  ret = uartWrite(ch, (uint8_t *)buf, len);

  va_end(args);


  return ret;
}

uint32_t uartGetBaud(uint8_t ch)
{
  return uart_tbl[ch].baud;
}

*/
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <stdarg.h>
#include "qbuffer.h"

const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

LOG_MODULE_REGISTER(uart_driver, CONFIG_UART_LOG_LEVEL);

#define UART_RX_BUF_SIZE 128
static uint8_t rx_buf[UART_RX_BUF_SIZE] = {0};
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];
static qbuffer_t uart_rx_qbuffer;

static bool is_open[UART_MAX_CH] = {false};
static uint32_t uart_baudrate[UART_MAX_CH] = {115200};

const struct device *uart[UART_MAX_CH] = {DEVICE_DT_GET(DT_NODELABEL(uart0))};

#define RECEIVE_TIMEOUT 100

static bool rx_enabled = false;
static bool rx_throttled = false;

static void interrupt_handler(const struct device *dev, void *user_data)
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

      qbufferWrite(&uart_rx_qbuffer, buffer, recv_len);
    }
  }
}

bool uartInit(void)
{
  int ret;

  // Initialize qbuffer for UART RX
  qbufferCreate(&uart_rx_qbuffer, uart_rx_buf, UART_RX_BUF_SIZE);

  // Check if CDC ACM device is ready
  if (!device_is_ready(uart_dev))
  {
    LOG_ERR("CDC ACM device not ready");
    return false;
  }

  while (true)
  {
    uint32_t dtr = 0U;

    uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
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
  ret = uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DCD, 1);
  if (ret)
  {
    LOG_WRN("Failed to set DCD, ret code %d", ret);
    return false;
  }

  ret = uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DSR, 1);
  if (ret)
  {
    LOG_WRN("Failed to set DSR, ret code %d", ret);
    return false;
  }

  /* Wait 100ms for the host to do all settings */
  k_msleep(100);

  uart_irq_callback_set(uart_dev, interrupt_handler);

  /* Enable rx interrupts */
  uart_irq_rx_enable(uart_dev);
  return true;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  int ret;

  if (ch >= UART_MAX_CH)
  {
    return false;
  }

  if (is_open[ch])
  {
    if (uart_baudrate[ch] == baud)
    {
      return true;
    }
  }

  if (ch == _DEF_UART1 && device_is_ready(uart_dev))
  {
    // Set baudrate if needed
    if (baud != uart_baudrate[ch])
    {
      ret = uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_BAUD_RATE, baud);
      if (ret)
      {
        LOG_ERR("Failed to set baudrate to %d", baud);
        return false;
      }
      uart_baudrate[ch] = baud;
    }

    is_open[ch] = true;
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

  return is_open[ch];
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
  {
    return false;
  }

  is_open[ch] = false;
  return true;
}

uint32_t uartAvailable(uint8_t ch)
{
  if (ch >= UART_MAX_CH || !is_open[ch])
  {
    return 0;
  }

  if (ch == _DEF_UART1)
  {
    return qbufferAvailable(&uart_rx_qbuffer);
  }

  return 0;
}

bool uartFlush(uint8_t ch)
{
  if (ch >= UART_MAX_CH || !is_open[ch])
  {
    return false;
  }

  if (ch == _DEF_UART1)
  {
    qbufferFlush(&uart_rx_qbuffer);
    return true;
  }

  return false;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;

  if (ch >= UART_MAX_CH || !is_open[ch])
  {
    return 0;
  }

  if (ch == _DEF_UART1)
  {
    if (qbufferAvailable(&uart_rx_qbuffer) > 0)
    {
      qbufferRead(&uart_rx_qbuffer, &ret, 1);
    }
  }

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{

  if (ch >= UART_MAX_CH || !is_open[ch] || p_data == NULL)
  {
    return 0;
  }

  if (ch == _DEF_UART1 && device_is_ready(uart_dev))
  {
    uart_fifo_fill(uart_dev, p_data, length);
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

  return uart_baudrate[ch];
}
#endif
