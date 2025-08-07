// main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "hw.h"
#include "ap.h"

LOG_MODULE_REGISTER(main);

int main(void) {
    LOG_INF("Starting USB HID device...");
    
    // 하드웨어 초기화
    hwInit();
    apInit();
    
    // 메인 루프
    while (1) {

        
        // // USB가 구성되어 있고 일시 중단되지 않은 경우에만 동작
        // if (usb_configured && !usb_suspended) {
        //     // 주기적으로 테스트 리포트 전송 (개발용)
        //     // send_keyboard_report();
        //     send_mouse_report();
        // }
        
        k_msleep(1000);
        
        // 어플리케이션 메인 처리
        // apMain();
    }

    return 0;
}
