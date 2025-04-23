// main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define REPORT_ID_KEYBOARD 0x01
#define REPORT_ID_MOUSE    0x02
#define REPORT_ID_VIA      0x03

static const struct device *hid_dev;

// HID Report Descriptor (Keyboard + Mouse + VIA)
static const uint8_t hid_report_desc[] = {
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

    // ----- Mouse Report Descriptor -----
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

    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size: 8 bits
    0x95, 0x40,       // Report Count: 64 bytes

    0x09, 0x62,       // Usage (Vendor IN report)
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x09, 0x63,       // Usage (Vendor OUT report)
    0x91, 0x02,       // Output (Data, Variable, Absolute)

    0xC0              // End Collection
};
static bool usb_configured = false;

// 콜백 함수 수정
static void hid_status_cb(enum usb_dc_status_code status, const uint8_t *param) {
    switch (status) {
    case USB_DC_CONFIGURED:
        usb_configured = true;
        LOG_INF("USB device configured");
        break;
    case USB_DC_DISCONNECTED:
        usb_configured = false;
        LOG_WRN("USB device disconnected");
        break;
    default:
        break;
    }
}

static void hid_ready_cb(const struct device *dev) {
    LOG_INF("HID IN ready");
}

static const struct hid_ops ops = {
    .int_in_ready = hid_ready_cb,
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
        0x10,            // X movement
        0x00,            // Y movement
        0x00             // Wheel
    };
    int ret = usb_write(0x81, report, sizeof(report), NULL);
    if (ret < 0) {
        LOG_ERR("Failed to send mouse report: %d", ret);
    }
}

int main(void) {
    // HID 디바이스 바인딩
    hid_dev = device_get_binding("HID_0");
    if (!hid_dev) {
        LOG_ERR("Failed to get HID device binding");
        return -1;
    }

    // HID 디스크립터 등록 및 초기화
    usb_hid_register_device(hid_dev, hid_report_desc, sizeof(hid_report_desc), &ops);
    usb_hid_init(hid_dev);
    // USB 장치 enable (HID는 정적으로 선언됨)
    int ret = usb_enable(hid_status_cb);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB");
        return ret;
    }

    LOG_INF("HID device ready");
    k_msleep(1000);
    while (1) {
        // send_keyboard_report();
        // k_msleep(1000);

        // send_mouse_report();
        k_msleep(1000);
    }

    return 0;
}
