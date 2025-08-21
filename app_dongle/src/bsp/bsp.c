/*
 * bsp.c
 *
 *  Created on: 2021. 1. 9.
 *      Author: baram
 */




#include "bsp.h"
#include <zephyr/sys/time_units.h>




void bspInit(void)
{
}

uint32_t bspGetCpuFreqMhz(void)
{
  // rtc_cpu_freq_config_t conf;
  // rtc_clk_cpu_freq_get_config(&conf);

  // return conf.freq_mhz;
  return 0;
}

void delay(uint32_t ms)
{
  k_msleep(ms);
}

uint32_t millis(void)
{
  return (uint32_t)(k_uptime_get());
}

uint32_t micros(void)
{
  uint32_t cycles;

  cycles = k_cycle_get_32();

  return (uint32_t)(k_cyc_to_us_floor32(cycles));
}

