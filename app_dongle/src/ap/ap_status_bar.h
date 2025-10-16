#ifndef AP_STATUS_BAR_H_
#define AP_STATUS_BAR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include "lvgl/lvgl.h"

/**
 * @brief 상태바 생성
 */
void apStatusBarCreate(void);

/**
 * @brief 왼쪽 키보드 연결 상태 업데이트
 * @param connected 연결 상태 (true: 연결, false: 연결 안됨)
 * @param battery 배터리 잔량 (0-100%)
 */
void apStatusBarUpdateLeftKeyboard(bool connected, uint8_t battery);

/**
 * @brief 오른쪽 키보드 연결 상태 업데이트
 * @param connected 연결 상태 (true: 연결, false: 연결 안됨)
 * @param battery 배터리 잔량 (0-100%)
 */
void apStatusBarUpdateRightKeyboard(bool connected, uint8_t battery);

/**
 * @brief USB 연결 상태 업데이트
 * @param connected USB 연결 상태 (true: 연결, false: 연결 안됨)
 */
void apStatusBarUpdateUSB(bool connected);

#ifdef __cplusplus
}
#endif

#endif /* AP_STATUS_BAR_H_ */
