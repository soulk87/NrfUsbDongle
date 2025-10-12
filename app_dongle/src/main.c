// main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "hw.h"
#include "ap.h"
#include <zephyr/device.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(main);

int main(void) {
    LOG_INF("Starting USB HID device...");
    
    // 하드웨어 초기화
    hwInit();
    apInit();

	char count_str[11] = {0};
	const struct device *display_dev;
	lv_obj_t *hello_world_label;
	lv_obj_t *count_label;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
	}

    display_blanking_off(display_dev);

    hello_world_label = lv_label_create(lv_scr_act());

	lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);
    // lv_timer_handler();
    // 메인 루프
    while (1) {

        
        // // USB가 구성되어 있고 일시 중단되지 않은 경우에만 동작
        // if (usb_configured && !usb_suspended) {
        //     // 주기적으로 테스트 리포트 전송 (개발용)
        //     // send_keyboard_report();
        //     send_mouse_report();
        // }
        lv_timer_handler();
        k_msleep(10);
        
        // 어플리케이션 메인 처리
        // apMain();
    }

    return 0;
}
