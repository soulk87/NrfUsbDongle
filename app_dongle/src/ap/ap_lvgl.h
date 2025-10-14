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

#ifdef __cplusplus
}
#endif

#endif /* AP_LVGL_H_ */
