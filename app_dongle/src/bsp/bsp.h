/*
 * bsp.h
 *
 *  Created on: 2021. 1. 9.
 *      Author: baram
 */

#ifndef MAIN_BSP_BSP_H_
#define MAIN_BSP_BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "def.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>


#if 0
#define _USE_LOG_PRINT    1

#if _USE_LOG_PRINT
#define logPrintf(fmt, ...)     printf(fmt, ##__VA_ARGS__)
#else
#define logPrintf(fmt, ...)
#endif
#endif

void logPrintf(const char *fmt, ...);


void bspInit(void);
uint32_t bspGetCpuFreqMhz(void);

void delay(uint32_t ms);
void delay_us(uint32_t us);
uint32_t millis(void);
uint32_t micros(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_BSP_BSP_H_ */
