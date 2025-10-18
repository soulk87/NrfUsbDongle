#ifndef AP_LVGL_H_
#define AP_LVGL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

/**
 * @brief LVGL 초기화
 */
void apLvglInit(void);

/**
 * @brief LVGL 스레드 시작
 */
void apLvglStart(void);

/**
 * @brief 레이어 변경 알림 (QMK에서 호출)
 * @param layer 현재 활성화된 레이어 번호
 */
void apLvglUpdateLayer(uint8_t layer);

/**
 * @brief 왼쪽 키보드 연결 상태 업데이트
 * @param connected 연결 상태 (true: 연결, false: 연결 안됨)
 * @param battery 배터리 잔량 (0-100%)
 */
void apLvglUpdateLeftKeyboard(bool connected, uint8_t battery);

/**
 * @brief 오른쪽 키보드 연결 상태 업데이트
 * @param connected 연결 상태 (true: 연결, false: 연결 안됨)
 * @param battery 배터리 잔량 (0-100%)
 */
void apLvglUpdateRightKeyboard(bool connected, uint8_t battery);

/**
 * @brief USB 연결 상태 업데이트
 * @param connected USB 연결 상태 (true: 연결, false: 연결 안됨)
 */
void apLvglUpdateUSB(bool connected);

#ifdef __cplusplus
}
#endif

#endif /* AP_LVGL_H_ */
