#include "cdc.h"
#include "qbuffer.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cdc, LOG_LEVEL_INF);

const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

static bool is_init = false;
static qbuffer_t rx_q;
static uint8_t rx_q_buf[1024];
static uint8_t rx_usb_buf[HW_CDC_RX_BUFFER_SIZE];
// 스레드 관련 변수 추가
#define THREAD_STACK_SIZE 512
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(usbcdc_thread_stack, THREAD_STACK_SIZE);
static struct k_thread usbcdc_thread_data;
static bool is_connected = false;


static void interrupt_handler(const struct device *dev, void *user_data)
{
  ARG_UNUSED(user_data);

  while (uart_irq_update(dev) && uart_irq_is_pending(dev))
  {
    if (uart_irq_rx_ready(dev))
    {
      int recv_len;

      // TODO: check qbuffer size is sufficient

      recv_len = uart_fifo_read(dev, rx_usb_buf, HW_CDC_RX_BUFFER_SIZE);
      if (recv_len < 0)
      {
        LOG_ERR("Failed to read UART FIFO");
        recv_len = 0;
      };

      if (recv_len == 0)
      {
        LOG_DBG("No data received from UART");
        continue;
      }
      qbufferWrite(&rx_q, rx_usb_buf, recv_len);
    }
  }
}

static void usbcdc_init_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  int ret;

  while (true) {
    uint32_t dtr = 0U;

    uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
    if (dtr) {
      break;
    } else {
      /* Give CPU resources to low priority threads. */
      k_sleep(K_MSEC(100));
    }
  }

  LOG_INF("DTR set");

  /* They are optional, we use them to test the interrupt endpoint */
  ret = uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DCD, 1);
  if (ret) {
    LOG_WRN("Failed to set DCD, ret code %d", ret);
  }

  ret = uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DSR, 1);
  if (ret) {
    LOG_WRN("Failed to set DSR, ret code %d", ret);
  }

  /* Wait 100ms for the host to do all settings */
  k_msleep(100);

  uart_irq_callback_set(uart_dev, interrupt_handler);

  /* Enable rx interrupts */
  uart_irq_rx_enable(uart_dev);

  is_connected = true;

}

static inline void print_baudrate(const struct device *dev)
{
  uint32_t baudrate;
  int ret;

  ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
  if (ret)
  {
    LOG_WRN("Failed to get baudrate, ret code %d", ret);
  }
  else
  {
    LOG_INF("Baudrate %u", baudrate);
  }
}


bool cdcInit(void)
{
  if (is_init)
  {
    LOG_WRN("CDC already initialized");
    return false;
  }

  qbufferCreate(&rx_q, rx_q_buf, 1024);

  if (!device_is_ready(uart_dev))
  {
    LOG_ERR("CDC ACM device not ready");
    return false;
  }

  k_thread_create(&usbcdc_thread_data, usbcdc_thread_stack,
                  K_THREAD_STACK_SIZEOF(usbcdc_thread_stack),
                  usbcdc_init_thread_func,
                  NULL, NULL, NULL,
                  THREAD_PRIORITY, 0, K_NO_WAIT);

  is_init = true;

  return is_init;
}

bool cdcIsInit(void)
{
  return is_init;
}

uint32_t cdcAvailable(void)
{
  uint32_t ret;
  if (is_connected)
  {
    ret = qbufferAvailable(&rx_q);
  }
  else
  {
    ret = 0;
  }

  return ret;
}

uint8_t cdcRead(void)
{
  uint8_t ret;

  qbufferRead(&rx_q, &ret, 1);

  return ret;
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t send_len;

  if (is_connected)
  {
    send_len = uart_fifo_fill(uart_dev, p_data, length);
  }
  else
  {
    send_len = 0;
  }

  return (uint32_t)send_len;
}

uint32_t cdcGetBaud(void)
{
  return 115200;
}