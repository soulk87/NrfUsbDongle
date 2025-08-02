// main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>
#include "hw.h"
#include "ap.h"

LOG_MODULE_REGISTER(main);

#define REPORT_ID_KEYBOARD 0x01
#define REPORT_ID_MOUSE    0x02
#define REPORT_ID_VIA      0x03

static const struct device *hid_dev;
static const struct device *hid_dev_1;
// HID Report Descriptor (Keyboard + Mouse + VIA)
static const uint8_t hid_report_desc[] = {
  //
  0x06, 0x60, 0xFF, // Usage Page (Vendor Defined)
  0x09, 0x61,       // Usage (Vendor Defined)
  0xA1, 0x01,       // Collection (Application)
  // Data to host
  0x09, 0x62,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 64,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x02,       //   Input (Data, Variable, Absolute)
  // Data from host
  0x09, 0x63,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 64,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x91, 0x02,       //   Output (Data, Variable, Absolute)
  0xC0              // End Collection
};


// HID Report Descriptor (Keyboard + Mouse + VIA)
static const uint8_t hid_report_desc_1[] = {
    // ----- Keyboard Report Descriptor -----
    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)

    0x85, REPORT_ID_KEYBOARD,  // Report ID = 0x01

    0x05, 0x07,       // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,       // Usage Minimum (Left Control)
    0x29, 0xE7,       // Usage Maximum (Right GUI)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size: 1 bit per modifier key
    0x95, 0x08,       // Report Count: 8 modifier keys
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count: 1 (reserved)
    0x75, 0x08,       // Report Size: 8 bits
    0x81, 0x03,       // Input (Constant)

    0x95, 0x06,       // Report Count: 6 keys
    0x75, 0x08,       // Report Size: 8 bits
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Keyboard/Keypad)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array)
    0xC0,             // End Collection

    // // ----- Mouse Report Descriptor -----
    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)

    0x85, REPORT_ID_MOUSE,  // Report ID = 0x02

    0x09, 0x01,       // Usage (Pointer)
    0xA1, 0x00,       // Collection (Physical)

    0x05, 0x09,       // Usage Page (Buttons)
    0x19, 0x01,       // Usage Minimum (Button 1)
    0x29, 0x03,       // Usage Maximum (Button 3)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x95, 0x03,       // Report Count: 3 buttons
    0x75, 0x01,       // Report Size: 1 bit
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count: 1 (padding)
    0x75, 0x05,       // Report Size: 5 bits
    0x81, 0x03,       // Input (Constant)

    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x30,       // Usage (X)
    0x09, 0x31,       // Usage (Y)
    0x09, 0x38,       // Usage (Wheel)
    0x15, 0x81,       // Logical Minimum (-127)
    0x25, 0x7F,       // Logical Maximum (127)
    0x75, 0x08,       // Report Size: 8 bits
    0x95, 0x03,       // Report Count: 3 (X, Y, Wheel)
    0x81, 0x06,       // Input (Data, Variable, Relative)

    0xC0,             // End Physical Collection
    0xC0,             // End Application Collection

    // ----- VIA Vendor-defined Report Descriptor -----
    0x06, 0x60, 0xFF, // Usage Page (Vendor-defined: 0xFF60)
    0x09, 0x61,       // Usage (Vendor Usage 0x61)
    0xA1, 0x01,       // Collection (Application)

    0x85, REPORT_ID_VIA, // Report ID = 0x03

    // Data to host
    0x09, 0x62,       //   Usage (Vendor Defined)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x95, 32,         //   Report Count
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    // Data from host
    0x09, 0x63,       //   Usage (Vendor Defined)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x95, 32,         //   Report Count
    0x75, 0x08,       //   Report Size (8)
    0x91, 0x02,       //   Output (Data, Variable, Absolute)
    0xC0              // End Collection
};

// HID Report Descriptor (Keyboard + Mouse + VIA)
static const uint8_t hid_report_desc_2[] = {
    // ----- Keyboard Report Descriptor -----
    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)

    0x85, REPORT_ID_KEYBOARD,  // Report ID = 0x01

    0x05, 0x07,       // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,       // Usage Minimum (Left Control)
    0x29, 0xE7,       // Usage Maximum (Right GUI)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size: 1 bit per modifier key
    0x95, 0x08,       // Report Count: 8 modifier keys
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count: 1 (reserved)
    0x75, 0x08,       // Report Size: 8 bits
    0x81, 0x03,       // Input (Constant)

    0x95, 0x06,       // Report Count: 6 keys
    0x75, 0x08,       // Report Size: 8 bits
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Keyboard/Keypad)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array)
    0xC0,             // End Collection

    // // ----- Mouse Report Descriptor -----
    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)

    0x85, REPORT_ID_MOUSE,  // Report ID = 0x02

    0x09, 0x01,       // Usage (Pointer)
    0xA1, 0x00,       // Collection (Physical)

    0x05, 0x09,       // Usage Page (Buttons)
    0x19, 0x01,       // Usage Minimum (Button 1)
    0x29, 0x03,       // Usage Maximum (Button 3)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x95, 0x03,       // Report Count: 3 buttons
    0x75, 0x01,       // Report Size: 1 bit
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count: 1 (padding)
    0x75, 0x05,       // Report Size: 5 bits
    0x81, 0x03,       // Input (Constant)

    0x05, 0x01,       // Usage Page (Generic Desktop Controls)
    0x09, 0x30,       // Usage (X)
    0x09, 0x31,       // Usage (Y)
    0x09, 0x38,       // Usage (Wheel)
    0x15, 0x81,       // Logical Minimum (-127)
    0x25, 0x7F,       // Logical Maximum (127)
    0x75, 0x08,       // Report Size: 8 bits
    0x95, 0x03,       // Report Count: 3 (X, Y, Wheel)
    0x81, 0x06,       // Input (Data, Variable, Relative)

    0xC0,             // End Physical Collection
    0xC0,             // End Application Collection
};

static bool usb_configured = false;
static bool usb_suspended = false;
static bool reconnect_needed = false;



// USB 연결 상태 확인 함수
static bool is_usb_ready(void) {
    return usb_configured && !usb_suspended;
}

// USB 연결 대기 함수 (타임아웃 포함)
static bool wait_for_usb_ready(uint32_t timeout_ms) {
    uint32_t start_time = k_uptime_get_32();
    
    while (!is_usb_ready()) {
        if (k_uptime_get_32() - start_time > timeout_ms) {
            LOG_WRN("USB ready timeout after %d ms", timeout_ms);
            return false;
        }
        k_msleep(10);
    }
    
    LOG_INF("USB ready after %d ms", k_uptime_get_32() - start_time);
    return true;
}

// VIA 데이터 처리 함수
static void process_via_data(const uint8_t *data, size_t len) {
    LOG_INF("Received VIA data:");
    for (size_t i = 0; i < len; i++) {
        LOG_INF("Byte %d: 0x%02X", i, data[i]);
    }

    // TODO: 데이터에 따른 동작 추가
}

// HID set_report 콜백
static int hid_set_report_cb(const struct device *dev,
                             struct usb_setup_packet *setup, int32_t *len,
                             uint8_t **data) {
    uint8_t report_id = (*data)[0];
    size_t report_len = *len;

    if (report_id == REPORT_ID_VIA) {
        LOG_INF("VIA report received");
        process_via_data(*data, report_len);
    } else {
        LOG_WRN("Unhandled report ID: 0x%02X", report_id);
    }
    return 0;
}

static int hid_get_report_cb(const struct device *dev,
                             struct usb_setup_packet *setup, int32_t *len,
                             uint8_t **data) {
    uint8_t report_id = setup->wValue & 0xFF; // Report ID
    LOG_INF("Get report request for ID: 0x%02X", report_id);

    if (report_id == REPORT_ID_VIA) {
        // VIA report 요청 처리
        static uint8_t via_report[64] = {0}; // 예시로 64바이트 VIA 리포트
        *data = via_report;
        *len = sizeof(via_report);
        LOG_INF("Returning VIA report");
    } else {
        LOG_WRN("Unhandled report ID: 0x%02X", report_id);
        return -ENOTSUP; // 지원하지 않는 리포트 ID
    } 
}

// USB 상태 콜백
static void hid_status_cb(enum usb_dc_status_code status, const uint8_t *param) {
    switch (status) {
    case USB_DC_CONFIGURED:
        usb_configured = true;
        usb_suspended = false;
        LOG_INF("USB device configured");
        break;
    case USB_DC_DISCONNECTED:
        usb_configured = false;
        LOG_WRN("USB device disconnected");
        // 재연결이 필요할 수 있음
        reconnect_needed = true;
        break;
    case USB_DC_SUSPEND:
        usb_suspended = true;
        LOG_INF("USB device suspended");
        break;
    case USB_DC_RESUME:
        usb_suspended = false;
        LOG_INF("USB device resumed");
        break;
    case USB_DC_RESET:
        LOG_INF("USB device reset");
        usb_configured = false;
        break;
    case USB_DC_ERROR:
        LOG_ERR("USB device error");
        reconnect_needed = true;
        break;
    default:
        LOG_DBG("USB status: %d", status);
        break;
    }
}

static void hid_ready_cb(const struct device *dev) {
    LOG_INF("HID IN ready");
}
static uint8_t rx_report[64] = {0};
static void hid_out_ready_cb(const struct device *dev) {
    LOG_DBG("HID Interrupt OUT endpoint ready");
    hid_int_ep_read(hid_dev_1, rx_report, sizeof(rx_report), NULL);
    LOG_HEXDUMP_DBG(rx_report, sizeof(rx_report), "HID Interrupt OUT report");
}

static const struct hid_ops ops = {
    .int_in_ready = hid_ready_cb,
    .int_out_ready = hid_out_ready_cb,
    .set_report = hid_set_report_cb, // set_report 콜백 추가
    .get_report = hid_get_report_cb, // get_report 콜백 추가
};

static void send_keyboard_report(void) {
    uint8_t report[] = {
        REPORT_ID_KEYBOARD, // Report ID
        0x00, // Modifier
        0x00, // Reserved
        0x04, // Keycode: 'a'
        0x00, 0x00, 0x00, 0x00, 0x00
    };
    int ret = usb_write(0x81, report, sizeof(report), NULL);  // IN endpoint 0x81 (default for HID)
    if (ret < 0) {
        LOG_ERR("Failed to send keyboard report: %d", ret);
    }
    k_msleep(50);

    report[3] = 0x00; // Key release
    ret = usb_write(0x81, report, sizeof(report), NULL);
    if (ret < 0) {
        LOG_ERR("Failed to send keyboard release: %d", ret);
    }
}

static void send_mouse_report(void) {
    uint8_t report[] = {
        REPORT_ID_MOUSE, // Report ID
        0x00,            // Buttons
        0x01,            // X movement
        0x00,            // Y movement
        0x00             // Wheel
    };
    int ret = hid_int_ep_write(hid_dev, report, sizeof(report), NULL);
    if (ret < 0) {
        LOG_ERR("Failed to send mouse report: %d", ret);
    }
}

// USB 재연결 함수
static void usb_reconnect(void) {
    LOG_INF("Attempting USB reconnection...");
    
    // USB 비활성화
    usb_disable();
    k_msleep(500); // 충분한 대기 시간
    
    // USB 재활성화
    int ret = usb_enable(hid_status_cb);
    if (ret == 0) {
        LOG_INF("USB reconnection successful");
        reconnect_needed = false;
    } else {
        LOG_ERR("USB reconnection failed: %d", ret);
    }
}

int main(void) {
    LOG_INF("Starting USB HID device...");
    
    // 하드웨어 초기화
    hwInit();
    apInit();
    
    // HID 디바이스 바인딩
    hid_dev = device_get_binding("HID_0");
    if (!hid_dev) {
        LOG_ERR("Failed to get HID device binding");
        return -1;
    }

    // HID 디스크립터 등록 및 초기화
    usb_hid_register_device(hid_dev, hid_report_desc_2, sizeof(hid_report_desc_2), &ops);
    
    int ret = usb_hid_init(hid_dev);
    if (ret != 0) {
        LOG_ERR("Failed to initialize HID device: %d", ret);
        return -1;
    }

    // 두 번째 HID 디바이스 바인딩
    hid_dev_1 = device_get_binding("HID_1");
    if (!hid_dev_1) {
        LOG_ERR("Failed to get second HID device binding");
        return -1;
    }
    
    // 두 번째 HID 디스크립터 등록 및 초기화
    usb_hid_register_device(hid_dev_1, hid_report_desc, sizeof(hid_report_desc), &ops);
    
    ret = usb_hid_init(hid_dev_1);
    if (ret != 0) {
        LOG_ERR("Failed to initialize second HID device: %d", ret);
        return -1;
    }

    // USB 활성화 (지연된 초기화)
    k_msleep(100); // 하드웨어 안정화 대기
    ret = usb_enable(hid_status_cb);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB: %d", ret);
        return ret;
    }

    LOG_INF("HID device ready");
    k_msleep(1000);
    
    // 메인 루프
    while (1) {
        // USB 재연결이 필요한 경우 처리
        if (reconnect_needed) {
            k_msleep(1000); // 재연결 전 대기
            usb_reconnect();
        }
        
        // USB가 구성되어 있고 일시 중단되지 않은 경우에만 동작
        if (usb_configured && !usb_suspended) {
            // 주기적으로 테스트 리포트 전송 (개발용)
            // send_keyboard_report();
            send_mouse_report();
        }
        
        k_msleep(1000);
        
        // 어플리케이션 메인 처리
        // apMain();
    }

    return 0;
}
