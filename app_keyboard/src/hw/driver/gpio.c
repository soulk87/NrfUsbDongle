/*
 * gpio.c
 *
 *  Created on: 2022. 9. 2.
 *      Author: baram
 */


#include "hw/include/gpio.h"
#include "hw/include/cli.h"


#ifdef _USE_HW_GPIO
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>


typedef struct
{
  const struct gpio_dt_spec gpio_spec;
  uint8_t       mode;
  bool          init_value;
} gpio_tbl_t;


static gpio_tbl_t gpio_tbl[GPIO_MAX_CH] =
    {
      {GPIO_DT_SPEC_GET(DT_NODELABEL(vcc_on), gpios), _DEF_OUTPUT, _DEF_HIGH }, // 0. VCC ON
      {GPIO_DT_SPEC_GET(DT_NODELABEL(pmw3610_rst), gpios), _DEF_OUTPUT, _DEF_HIGH }, // 1. PMW3610 RST
    };

static uint8_t gpio_data[GPIO_MAX_CH];


#ifdef _USE_HW_CLI
static void cliGpio(cli_args_t *args);
#endif



bool gpioInit(void)
{
  bool ret = true;

  // Initialize each pin based on its mode
  for (int i=0; i<GPIO_MAX_CH; i++)
  {
    gpioPinMode(i, gpio_tbl[i].mode);
    gpioPinWrite(i, gpio_tbl[i].init_value);
  }

#ifdef _USE_HW_CLI
  cliAdd("gpio", cliGpio);
#endif

  return ret;
}

bool gpioPinMode(uint8_t ch, uint8_t mode)
{
  bool ret = true;

  if (ch >= GPIO_MAX_CH)
  {
    return false;
  }

  gpio_flags_t flags = 0;
  
  switch(mode)
  {
    case _DEF_INPUT:
      flags = GPIO_INPUT;
      break;

    case _DEF_INPUT_PULLUP:
      flags = GPIO_INPUT | GPIO_PULL_UP;
      break;

    case _DEF_INPUT_PULLDOWN:
      flags = GPIO_INPUT | GPIO_PULL_DOWN;
      break;

    case _DEF_OUTPUT:
      flags = GPIO_OUTPUT;
      break;

    case _DEF_OUTPUT_PULLUP:
      flags = GPIO_OUTPUT | GPIO_PULL_UP;
      break;

    case _DEF_OUTPUT_PULLDOWN:
      flags = GPIO_OUTPUT | GPIO_PULL_DOWN;
      break;
  }

  gpio_pin_configure(gpio_tbl[ch].gpio_spec.port, gpio_tbl[ch].gpio_spec.pin, flags);
  
  return ret;
}

void gpioPinWrite(uint8_t ch, uint8_t value)
{
  if (ch >= GPIO_MAX_CH)
  {
    return;
  }

  gpio_data[ch] = value;
  gpio_pin_set(gpio_tbl[ch].gpio_spec.port, gpio_tbl[ch].gpio_spec.pin, value);
}

uint8_t gpioPinRead(uint8_t ch)
{
  uint8_t ret = 0;

  if (ch >= GPIO_MAX_CH)
  {
    return false;
  }

  ret = gpio_pin_get(gpio_tbl[ch].gpio_spec.port, gpio_tbl[ch].gpio_spec.pin);
  return ret;
}

void gpioPinToggle(uint8_t ch)
{
  if (ch >= GPIO_MAX_CH)
  {
    return;
  }

  gpio_data[ch] = !gpio_data[ch];
  gpio_pin_toggle(gpio_tbl[ch].gpio_spec.port, gpio_tbl[ch].gpio_spec.pin);
}





#ifdef _USE_HW_CLI
void cliGpio(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "show") == true)
  {
    while(cliKeepLoop())
    {
      for (int i=0; i<GPIO_MAX_CH; i++)
      {
        cliPrintf("%d", gpioPinRead(i));
      }
      cliPrintf("\n");
      k_msleep(100);
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "read") == true)
  {
    uint8_t ch;

    ch = (uint8_t)args->getData(1);

    while(cliKeepLoop())
    {
      cliPrintf("gpio read %d : %d\n", ch, gpioPinRead(ch));
      k_msleep(100);
    }

    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "write") == true)
  {
    uint8_t ch;
    uint8_t data;

    ch   = (uint8_t)args->getData(1);
    data = (uint8_t)args->getData(2);

    gpioPinWrite(ch, data);

    cliPrintf("gpio write %d : %d\n", ch, data);
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("gpio show\n");
    cliPrintf("gpio read ch[0~%d]\n", GPIO_MAX_CH-1);
    cliPrintf("gpio write ch[0~%d] 0:1\n", GPIO_MAX_CH-1);
  }
}
#endif


#endif