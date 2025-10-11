#include "usb.h"

#ifdef _USE_HW_USB
// #include "usb_hid/usbd_hid.h"
#include "cdc.h"
#include "cli.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(myusb);

static bool is_init = false;

// semaphore for USB operations
K_SEM_DEFINE(usb_setting_sema, 1, 1); // 초기 카운트 1, 최대 카운트 1 (Binary Semaphore)

static bool usb_configured = false;
static bool usb_suspended = false;
static bool reconnect_needed = false;

// 스레드 관련 변수 추가
#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(usb_thread_stack, THREAD_STACK_SIZE);
static struct k_thread usb_thread_data;

#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif

// USB 상태 콜백
static void hid_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
  switch (status)
  {
  case USB_DC_CONFIGURED:
    // semephore를 사용하여 USB 작업 동기화
    k_sem_take(&usb_setting_sema, K_FOREVER);
    usb_configured = true;
    usb_suspended = false;
    k_sem_give(&usb_setting_sema);
    LOG_INF("USB device configured");
    break;
  case USB_DC_DISCONNECTED:
    k_sem_take(&usb_setting_sema, K_FOREVER);
    usb_configured = false;
    reconnect_needed = true;
    k_sem_give(&usb_setting_sema);
    LOG_WRN("USB device disconnected");
    break;
  case USB_DC_SUSPEND:
    k_sem_take(&usb_setting_sema, K_FOREVER);
    usb_suspended = true;
    k_sem_give(&usb_setting_sema);
    LOG_INF("USB device suspended");
    break;
  case USB_DC_RESUME:
    k_sem_take(&usb_setting_sema, K_FOREVER);
    usb_suspended = false;
    k_sem_give(&usb_setting_sema);
    LOG_INF("USB device resumed");
    break;
  case USB_DC_RESET:
    LOG_INF("USB device reset");
    k_sem_take(&usb_setting_sema, K_FOREVER);
    usb_configured = false;
    k_sem_give(&usb_setting_sema);
    break;
  case USB_DC_ERROR:
    LOG_ERR("USB device error");
    k_sem_take(&usb_setting_sema, K_FOREVER);
    reconnect_needed = true;
    k_sem_give(&usb_setting_sema);
    break;
  default:
    LOG_DBG("USB status: %d", status);
    break;
  }
}

// USB 재연결 함수
static void usb_reconnect(void)
{
  LOG_INF("Attempting USB reconnection...");

  // USB 비활성화
  usb_disable();
  k_msleep(500); // 충분한 대기 시간

  // USB 재활성화
  int ret = usb_enable(hid_status_cb);
  if (ret == 0)
  {
    LOG_INF("USB reconnection successful");
    reconnect_needed = false;
  }
  else
  {
    LOG_ERR("USB reconnection failed: %d", ret);
  }
}

static void usb_thread_func(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1)
  {
    // USB 재연결이 필요한 경우 처리
    if (reconnect_needed)
    {
      usb_reconnect();
    }
    k_msleep(500);
  }
}

bool usbInit(void)
{
  bool ret = false;
  ret |= cdcInit();
  k_msleep(500); // 하드웨어 안정화 대기
  // ret |= usbHidInit();
  is_init = ret;
  // USB 활성화 (지연된 초기화)
  // k_msleep(500); // 하드웨어 안정화 대기
  ret = usb_enable(hid_status_cb);
  if (ret != 0)
  {
    LOG_ERR("Failed to enable USB: %d", ret);
    return ret;
  }

  LOG_INF("HID device ready");
  // k_msleep(1000);
  k_thread_create(&usb_thread_data, usb_thread_stack,
                  K_THREAD_STACK_SIZEOF(usb_thread_stack),
                  usb_thread_func,
                  NULL, NULL, NULL,
                  THREAD_PRIORITY, 0, K_NO_WAIT);

#ifdef _USE_HW_CLI
  cliAdd("usb", cliCmd);
#endif
  return true;
}

bool usbIsOpen(void)
{
  return true;
}

bool usbIsConnect(void)
{
  bool ret = false;
  k_sem_take(&usb_setting_sema, K_FOREVER);
  if (usb_configured && !usb_suspended)
  {
    ret = true;
  }
  k_sem_give(&usb_setting_sema);
  return ret;
}

bool usbIsSuspended(void)
{
  // return tud_suspended();
  return false; // 현재는 일시 중단 상태를 사용하지 않음
}

#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    // uint16_t vid = 0;
    // uint16_t pid = 0;
    // uint8_t *p_data;
    // uint16_t len = 0;

    // if (p_desc != NULL)
    // {
    //   p_data = p_desc->GetDeviceDescriptor(USBD_SPEED_HIGH, &len);
    //   vid = (p_data[ 9]<<8)|(p_data[ 8]<<0);
    //   pid = (p_data[11]<<8)|(p_data[10]<<0);
    // }

    // cliPrintf("USB PID     : 0x%04X\n", vid);
    // cliPrintf("USB VID     : 0x%04X\n", pid);

    while (cliKeepLoop())
    {
      // cliPrintf("mounted   : %d\n", tud_mounted());
      // cliPrintf("connedted : %d\n", tud_connected());
      // cliPrintf("tud_suspended : %d\n", tud_suspended());

      // cliPrintf("USB Connect : %d\n", usbIsConnect());
      // cliPrintf("USB Open    : %d\n", usbIsOpen());
      // cliMoveUp(5);
      delay(100);
    }
    cliMoveDown(5);

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test"))
  {
    uint8_t keycode[6] = {0};
    keycode[0] = HID_KEY_A;

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usb info\n");
  }
}
#endif

#endif