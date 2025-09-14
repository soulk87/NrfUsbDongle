#include "usbd_hid.h"

#ifdef _USE_HW_USB

#define KBD_NAME "BARAM-45K-ESP32"

#define USB_VID 0x0483
#define USB_PID 0x5300

#define USB_HID_LOG 0

#if USB_HID_LOG == 1
#define logDebug(...)       \
  {                         \
    logPrintf(__VA_ARGS__); \
  }
#else
#define logDebug(...)
#endif

#include "cli.h"
#include "log.h"
#include "keys.h"
#include "qbuffer.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>
#include "driver/usb/usb.h"

LOG_MODULE_REGISTER(my_usb_hid);

#define REPORT_ID_KEYBOARD 0x01
#define REPORT_ID_MOUSE 0x02
#define REPORT_ID_VIA 0x03

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(usb_thread_stack, THREAD_STACK_SIZE);
static struct k_thread usb_thread_data;
static void hidThread(void *arg1, void *arg2, void *arg3);


static const struct device *hid_dev;
static const struct device *hid_dev_via;

// HID Report Descriptor (Keyboard + Mouse)
static const uint8_t hid_report_desc[] = {
    // ----- Keyboard Report Descriptor -----
    0x05, 0x01, // Usage Page (Generic Desktop Controls)
    0x09, 0x06, // Usage (Keyboard)
    0xA1, 0x01, // Collection (Application)

    0x85, REPORT_ID_KEYBOARD, // Report ID = 0x01

    0x05, 0x07, // Usage Page (Keyboard/Keypad)
    0x19, 0xE0, // Usage Minimum (Left Control)
    0x29, 0xE7, // Usage Maximum (Right GUI)
    0x15, 0x00, // Logical Minimum (0)
    0x25, 0x01, // Logical Maximum (1)
    0x75, 0x01, // Report Size: 1 bit per modifier key
    0x95, 0x08, // Report Count: 8 modifier keys
    0x81, 0x02, // Input (Data, Variable, Absolute)

    0x95, 0x01, // Report Count: 1 (reserved)
    0x75, 0x08, // Report Size: 8 bits
    0x81, 0x03, // Input (Constant)

    0x95, 0x06, // Report Count: 6 keys
    0x75, 0x08, // Report Size: 8 bits
    0x15, 0x00, // Logical Minimum (0)
    0x25, 0x65, // Logical Maximum (101)
    0x05, 0x07, // Usage Page (Keyboard/Keypad)
    0x19, 0x00, // Usage Minimum (0)
    0x29, 0x65, // Usage Maximum (101)
    0x81, 0x00, // Input (Data, Array)
    0xC0,       // End Collection

    // // ----- Mouse Report Descriptor -----
    0x05, 0x01, // Usage Page (Generic Desktop Controls)
    0x09, 0x02, // Usage (Mouse)
    0xA1, 0x01, // Collection (Application)

    0x85, REPORT_ID_MOUSE, // Report ID = 0x02

    0x09, 0x01, // Usage (Pointer)
    0xA1, 0x00, // Collection (Physical)

    0x05, 0x09, // Usage Page (Buttons)
    0x19, 0x01, // Usage Minimum (Button 1)
    0x29, 0x03, // Usage Maximum (Button 3)
    0x15, 0x00, // Logical Minimum (0)
    0x25, 0x01, // Logical Maximum (1)
    0x95, 0x03, // Report Count: 3 buttons
    0x75, 0x01, // Report Size: 1 bit
    0x81, 0x02, // Input (Data, Variable, Absolute)

    0x95, 0x01, // Report Count: 1 (padding)
    0x75, 0x05, // Report Size: 5 bits
    0x81, 0x03, // Input (Constant)

    0x05, 0x01, // Usage Page (Generic Desktop Controls)
    0x09, 0x30, // Usage (X)
    0x09, 0x31, // Usage (Y)
    0x09, 0x38, // Usage (Wheel)
    0x15, 0x81, // Logical Minimum (-127)
    0x25, 0x7F, // Logical Maximum (127)
    0x75, 0x08, // Report Size: 8 bits
    0x95, 0x03, // Report Count: 3 (X, Y, Wheel)
    0x81, 0x06, // Input (Data, Variable, Relative)

    0xC0, // End Physical Collection
    0xC0, // End Application Collection
};
// HID Report Descriptor (VIA)
static const uint8_t hid_report_desc_via[] = {
    //
    0x06, 0x60, 0xFF, // Usage Page (Vendor Defined)
    0x09, 0x61,       // Usage (Vendor Defined)
    0xA1, 0x01,       // Collection (Application)
    // Data to host
    0x09, 0x62,       //   Usage (Vendor Defined)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x95, HID_VIA_EP_SIZE,         //   Report Count
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    // Data from host
    0x09, 0x63,       //   Usage (Vendor Defined)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x95, HID_VIA_EP_SIZE,         //   Report Count
    0x75, 0x08,       //   Report Size (8)
    0x91, 0x02,       //   Output (Data, Variable, Absolute)
    0xC0              // End Collection
};

static void (*via_hid_receive_func)(uint8_t *data, uint8_t length) = NULL;

static uint8_t via_hid_usb_report[HID_VIA_EP_SIZE];

static qbuffer_t report_q;


#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif

// HID set_report 콜백
static int hid_set_report_cb(const struct device *dev,
                             struct usb_setup_packet *setup, int32_t *len,
                             uint8_t **data)
{

  return 0;
}

static int hid_get_report_cb(const struct device *dev,
                             struct usb_setup_packet *setup, int32_t *len,
                             uint8_t **data)
{

  return 0;
}

static uint8_t rx_report[HID_VIA_EP_SIZE] = {0};

static void hid_out_ready_cb(const struct device *dev)
{
  LOG_DBG("HID Interrupt OUT endpoint ready");
  hid_int_ep_read(hid_dev_via, rx_report, sizeof(rx_report), NULL);
  LOG_HEXDUMP_DBG(rx_report, sizeof(rx_report), "HID Interrupt OUT report");

  memcpy(via_hid_usb_report, rx_report, HID_VIA_EP_SIZE);
  if (via_hid_receive_func != NULL)
  {
    via_hid_receive_func(via_hid_usb_report, HID_VIA_EP_SIZE);
  }

  int ret = hid_int_ep_write(hid_dev_via, via_hid_usb_report, sizeof(via_hid_usb_report), NULL);
  if (ret < 0)
  {
    LOG_ERR("Failed to send mouse report: %d", ret);
  }
}

static const struct hid_ops hid_ops = {
    .set_report = hid_set_report_cb, // set_report 콜백 추가
    .get_report = hid_get_report_cb, // get_report 콜백 추가
};

static const struct hid_ops via_ops = {
    .int_out_ready = hid_out_ready_cb,
    .set_report = hid_set_report_cb, // set_report 콜백 추가
    .get_report = hid_get_report_cb, // get_report 콜백 추가
};

static void send_keyboard_report(void)
{
  uint8_t report[] = {
      REPORT_ID_KEYBOARD, // Report ID
      0x00,               // Modifier
      0x00,               // Reserved
      0x04,               // Keycode: 'a'
      0x00, 0x00, 0x00, 0x00, 0x00};
  int ret = usb_write(0x81, report, sizeof(report), NULL); // IN endpoint 0x81 (default for HID)
  if (ret < 0)
  {
    LOG_ERR("Failed to send keyboard report: %d", ret);
  }
  k_msleep(50);

  report[3] = 0x00; // Key release
  ret = usb_write(0x81, report, sizeof(report), NULL);
  if (ret < 0)
  {
    LOG_ERR("Failed to send keyboard release: %d", ret);
  }
}

static void send_mouse_report(void)
{
  uint8_t report[] = {
      REPORT_ID_MOUSE, // Report ID
      0x00,            // Buttons
      0x01,            // X movement
      0x00,            // Y movement
      0x00             // Wheel
  };
  int ret = hid_int_ep_write(hid_dev, report, sizeof(report), NULL);
  if (ret < 0)
  {
    LOG_ERR("Failed to send mouse report: %d", ret);
  }
}

bool usbHidInit(void)
{
  bool ret = true;

  // HID 디바이스 바인딩
  hid_dev = device_get_binding("HID_0");
  if (!hid_dev)
  {
    LOG_ERR("Failed to get HID device binding");
    return false;
  }

  // HID 디스크립터 등록 및 초기화
  usb_hid_register_device(hid_dev, hid_report_desc, sizeof(hid_report_desc), &hid_ops);

  ret = usb_hid_init(hid_dev);
  if (ret != 0)
  {
    LOG_ERR("Failed to initialize HID device: %d", ret);
    return false;
  }

  // 두 번째 HID 디바이스 바인딩
  hid_dev_via = device_get_binding("HID_1");
  if (!hid_dev_via)
  {
    LOG_ERR("Failed to get second HID device binding");
    return false;
  }

  // 두 번째 HID 디스크립터 등록 및 초기화
  usb_hid_register_device(hid_dev_via, hid_report_desc_via, sizeof(hid_report_desc_via), &via_ops);

  ret = usb_hid_init(hid_dev_via);
  if (ret != 0)
  {
    LOG_ERR("Failed to initialize second HID device: %d", ret);
    return false;
  }

  k_thread_create(&usb_thread_data, usb_thread_stack,
                K_THREAD_STACK_SIZEOF(usb_thread_stack),
                hidThread,
                NULL, NULL, NULL,
                THREAD_PRIORITY, 0, K_NO_WAIT);

#ifdef _USE_HW_CLI
  cliAdd("usbhid", cliCmd);
#endif
  return ret;
}

bool usbHidSetViaReceiveFunc(void (*func)(uint8_t *, uint8_t))
{
  via_hid_receive_func = func;
  return true;
}

bool usbHidSendReport(uint8_t *p_data, uint16_t length)
{
  bool ret = true;

  // if (!tud_suspended())
  // {
  //   report_info_t report_info;

  //   memcpy(report_info.buf, p_data, HID_KEYBOARD_REPORT_SIZE);
  //   qbufferWrite(&report_q, (uint8_t *)&report_info, 1);
  // }
  // else
  // {
  //   tud_remote_wakeup();
  //   ret = false;
  // }
  if (length > 8)
    return false;
    
  static uint8_t report[9] = {0};
  report[0] = REPORT_ID_KEYBOARD;
  memcpy(report + 1, p_data, length);

  int usb_write_ret = hid_int_ep_write(hid_dev, report, sizeof(report), NULL);
  if (usb_write_ret < 0)
  {
    LOG_ERR("Failed to send keyboard report: %d", usb_write_ret);
    ret = false;
  }

  
  return ret;
}

bool usbHidSendReportEXK(uint8_t *p_data, uint16_t length)
{
  // exk_report_info_t report_info;

  // if (length > HID_EXK_EP_SIZE)
  //   return false;

  // if (!USBD_is_suspended())
  // {
  //   memcpy(hid_buf_exk, p_data, length);
  //   if (!USBD_HID_SendReportEXK((uint8_t *)hid_buf_exk, length))
  //   {
  //     report_info.len = length;
  //     memcpy(report_info.buf, p_data, length);
  //     qbufferWrite(&report_exk_q, (uint8_t *)&report_info, 1);
  //   }
  // }
  // else
  // {
  //   usbHidUpdateWakeUp(&USBD_Device);
  // }

  return true;
}

static void hidThread(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1)
  {
    if (usbIsConnect())
    {
      send_mouse_report();
    }
    k_msleep(1000);
  }
}

#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usbhid info\n");
  }
}
#endif

#endif // _USE_HW_USB