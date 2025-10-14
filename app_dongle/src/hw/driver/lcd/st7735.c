/*
 * st7735.c
 *
 *  Created on: 2020. 10. 24.
 *      Author: baram
 */

#include "hw/include/spi.h"
#include "hw/include/gpio.h"
#include "hw/include/cli.h"
#include "lcd/st7735.h"
#include "lcd/st7735_regs.h"

#ifdef _USE_HW_ST7735
// #include "FreeRTOS.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef _USE_HW_PWM
#include "hw/include/pwm.h"
#endif

#define _PIN_DEF_BKT HW_PWM_LCD_BLK // gpio로 설정 되어 있지 않은 경우. pwm으로 제어
#define _PIN_DEF_DC HW_GPIO_PIN_LCD_DC
#define _PIN_DEF_CS -1 // 실행 되지 않도록 설정
#define _PIN_DEF_RST HW_GPIO_PIN_LCD_RST

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_ML 0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH 0x04

static uint8_t spi_ch = HW_SPI_CH_LCD;

static const int32_t _width = HW_LCD_WIDTH;
static const int32_t _height = HW_LCD_HEIGHT;
static void (*frameCallBack)(void) = NULL;
volatile static bool is_write_frame = false;

#ifdef HW_ST7735_128X128
const uint32_t colstart = 1;
const uint32_t rowstart = 2;
#else //80x160
const uint32_t colstart = 1;
const uint32_t rowstart = 26;
#endif

static void writecommand(uint8_t c);
static void writedata(uint8_t d);
static void st7735InitRegs(void);
// static void st7735FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
static void st7735SetRotation(uint8_t m);
static bool st7735Reset(void);

// static DMA_ATTR [HW_LCD_WIDTH * HW_LCD_HEIGHT * 2];
// char * dma_data = heap_caps_malloc(ByteLength, MALLOC_CAP_DMA);
// memcpy(dma_data,buf,ByteLength);
// DMA_ATTR uint8_t LCD_DMA_ColorBuffer[HW_LCD_WIDTH * HW_LCD_HEIGHT * 2];
// static uint8_t* dma_data_buffer;
// uint8_t* LCD_DMA_ColorBuffer;
#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif
static void TransferDoneISR(void)
{
    if (is_write_frame == true)
    {
        is_write_frame = false;
        if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);

        if (frameCallBack != NULL)
        {
            frameCallBack();
        }
    }
}

void st7735SetBacklight(uint8_t value) // 0~100%
{

    if(HW_PWM_LCD_BLK < 0 ) return;

#ifdef _USE_HW_PWM
    // pwm로 되어있는 경우
    pwmWrite(_PIN_DEF_BKT,(uint16_t)value);
#else
    // gpio로 되어있는 경우
    if (value > 0)
        gpioPinWrite(_PIN_DEF_BKT, _DEF_HIGH);
    else
        gpioPinWrite(_PIN_DEF_BKT, _DEF_LOW);
#endif
}

bool st7735Init(void)
{
    // LCD_DMA_ColorBuffer = heap_caps_malloc(HW_LCD_WIDTH * HW_LCD_HEIGHT * 2, MALLOC_CAP_DMA);

    bool ret;

    ret = st7735Reset();

#ifdef _USE_HW_CLI
    cliAdd("st7735", cliCmd);
#endif

    return ret;
}

// bool st7735InitDriver(lcd_driver_t *p_driver)
// {
//     p_driver->init = st7735Init;
//     p_driver->reset = st7735Reset;
//     p_driver->setWindow = st7735SetWindow;
//     p_driver->getWidth = st7735GetWidth;
//     p_driver->getHeight = st7735GetHeight;
//     p_driver->setCallBack = st7735SetCallBack;
//     p_driver->sendBuffer = st7735SendBuffer;
//     return true;
// }

bool st7735Reset(void)
{
    if(_PIN_DEF_RST >=0)
    {
        gpioPinWrite(_PIN_DEF_RST, _DEF_LOW);
        delay(50);
        gpioPinWrite(_PIN_DEF_RST, _DEF_HIGH);
    }

    spiAttachPostCallback(spi_ch, TransferDoneISR);
    spiBegin(spi_ch);

    st7735SetBacklight(0);
    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
    delay(10);

    st7735InitRegs();

    st7735SetRotation(3);
    // st7735FillRect(0, 0, _width, _height, 0x001F);
    st7735SetBacklight(100);
    return true;
}

uint16_t st7735GetWidth(void)
{
    return HW_LCD_WIDTH;
}

uint16_t st7735GetHeight(void)
{
    return HW_LCD_HEIGHT;
}

void writecommand(uint8_t c)
{
    gpioPinWrite(_PIN_DEF_DC, _DEF_LOW);
    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    spiTransfer8(spi_ch, c);

    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
}

void writedata(uint8_t d)
{
    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    spiTransfer8(spi_ch, d);

    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
}

void st7735InitRegs(void)
{
    writecommand(ST7735_SWRESET); //  1: Software reset, 0 args, w/delay
    delay(50);

    writecommand(ST7735_SLPOUT); //  2: Out of sleep mode, 0 args, w/delay
    delay(100);

    writecommand(ST7735_FRMCTR1); //  3: Frame rate ctrl - normal mode, 3 args:
    writedata(0x01);              //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    writedata(0x2C);
    writedata(0x2D);

    writecommand(ST7735_FRMCTR2); //  4: Frame rate control - idle mode, 3 args:
    writedata(0x01);              //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    writedata(0x2C);
    writedata(0x2D);

    writecommand(ST7735_FRMCTR3); //  5: Frame rate ctrl - partial mode, 6 args:
    writedata(0x01);              //     Dot inversion mode
    writedata(0x2C);
    writedata(0x2D);
    writedata(0x01); //     Line inversion mode
    writedata(0x2C);
    writedata(0x2D);

    writecommand(ST7735_INVCTR); //  6: Display inversion ctrl, 1 arg, no delay:
    writedata(0x07);             //     No inversion

    writecommand(ST7735_PWCTR1); //  7: Power control, 3 args, no delay:
    writedata(0xA2);
    writedata(0x02); //     -4.6V
    writedata(0x84); //     AUTO mode

    writecommand(ST7735_PWCTR2); //  8: Power control, 1 arg, no delay:
    writedata(0xC5);             //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD

    writecommand(ST7735_PWCTR3); //  9: Power control, 2 args, no delay:
    writedata(0x0A);             //     Opamp current small
    writedata(0x00);             //     Boost frequency

    writecommand(ST7735_PWCTR4); // 10: Power control, 2 args, no delay:
    writedata(0x8A);             //     BCLK/2, Opamp current small & Medium low
    writedata(0x2A);

    writecommand(ST7735_PWCTR5); // 11: Power control, 2 args, no delay:
    writedata(0x8A);
    writedata(0xEE);

    writecommand(ST7735_VMCTR1); // 12: Power control, 1 arg, no delay:
    writedata(0x0E);

    writecommand(ST7735_INVOFF); // 13: Don't invert display, no args, no delay

    // writecommand(ST7735_MADCTL); // 14: Memory access control (directions), 1 arg:
    // writedata(0xA8);             //     row addr/col addr, bottom to top refresh

    writecommand(ST7735_COLMOD); // 15: set color mode, 1 arg, no delay:
    writedata(0x05);             //     16-bit color

    writecommand(ST7735_CASET); //  1: Column addr set, 4 args, no delay:
    writedata(0x00);
    writedata(0x00 + colstart); // XSTART
    writedata(0x00);
    writedata(HW_LCD_WIDTH + colstart -1); // XEND

    writecommand(ST7735_RASET); // 2: Row addr set, 4 args, no delay:
    writedata(0x00);
    writedata(0x00 + rowstart); // YSTART
    writedata(0x00);
    writedata(HW_LCD_HEIGHT + rowstart -1); // YEND

    writecommand(ST7735_NORON); //  3: Normal display on, no args, w/delay
    delay(10);
    writecommand(ST7735_DISPON); //  4: Main screen turn on, no args w/delay
    delay(10);
}

void st7735SetRotation(uint8_t mode)
{
    writecommand(ST7735_MADCTL);

    switch (mode)
    {
    case 0:
        writedata(MADCTL_MX | MADCTL_MY | MADCTL_BGR);
        break;

    case 1:
        writedata(MADCTL_MY | MADCTL_MV | MADCTL_BGR);
        break;

    case 2:
        writedata(MADCTL_BGR);
        break;

    case 3:
        writedata(MADCTL_MX | MADCTL_MV | MADCTL_BGR);
        break;

    case 4:
        writedata(MADCTL_MY | MADCTL_MV | MADCTL_BGR);
        break;
    }
}

void st7735SetWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    writecommand(ST7735_CASET); // Column addr set
    writedata(0x00);
    writedata(x0 + colstart); // XSTART
    writedata(0x00);
    writedata(x1 + colstart); // XEND

    writecommand(ST7735_RASET); // Row addr set
    writedata(0x00);
    writedata(y0 + rowstart); // YSTART
    writedata(0x00);
    writedata(y1 + rowstart); // YEND

    writecommand(ST7735_RAMWR); // write to RAM
}

// void st7735FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
// {
//     // uint16_t line_buf[w];

//     // Clipping
//     if ((x >= _width) || (y >= _height))
//         return;

//     if (x < 0)
//     {
//         w += x;
//         x = 0;
//     }
//     if (y < 0)
//     {
//         h += y;
//         y = 0;
//     }

//     if ((x + w) > _width)
//         w = _width - x;
//     if ((y + h) > _height)
//         h = _height - y;

//     if ((w < 1) || (h < 1))
//         return;

//     st7735SetWindow(x, y, x + w - 1, y + h - 1);
// //    spiSetBitWidth(spi_ch, 16);

//     gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
//     if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

//     for (int LineRow = 0; LineRow < h; LineRow++)
//     {
//         for (int LineCol = 0; LineCol < w; LineCol++)
//         {
//             int pos = LineRow*w + LineCol;
//             *((uint8_t *)LCD_DMA_ColorBuffer + pos * 2 + 0) = color >> 8;
//             *((uint8_t *)LCD_DMA_ColorBuffer + pos * 2 + 1) = color;
//         }
//     }

//     // spiTransfer(spi_ch, LCD_DMA_ColorBuffer, NULL, h * w * 2);
//     spiDmaTxTransfer(spi_ch, LCD_DMA_ColorBuffer,  h * w * 2, 99999);

//     if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_HIGH);
// }

bool st7735SendBuffer(uint8_t *p_data, uint32_t length, uint32_t timeout_ms)
{
    is_write_frame = true;

//    spiSetBitWidth(spi_ch, 16);

    gpioPinWrite(_PIN_DEF_DC, _DEF_HIGH);
    if(_PIN_DEF_CS >= 0) gpioPinWrite(_PIN_DEF_CS, _DEF_LOW);

    spiDmaTxTransfer(spi_ch, (void *)p_data, length, 99999);
    return true;
}

bool st7735SetCallBack(void (*p_func)(void))
{
    frameCallBack = p_func;

    return true;
}

#ifdef _USE_HW_CLI

static void frameISR(void)
{
    static int i = 0;

    cliPrintf("done %d\n", i++);
}

void cliCmd(cli_args_t *args)
{
    bool ret = false;

    if (args->argc == 1 && args->isStr(0, "info"))
    {
        cliPrintf("Width  : %d\n", _width);
        cliPrintf("Heigth : %d\n", _height);
        ret = true;
    }

    if (args->argc == 1 && args->isStr(0, "test"))
    {
        uint32_t pre_time;
        uint32_t pre_time_draw;
        uint16_t color_tbl[3] = {0xF800, 0x07E0, 0x001F};
        uint8_t color_index = 0;
        void (*cb_func_pre)(void);

        cb_func_pre = frameCallBack;

        st7735SetCallBack(frameISR);

        pre_time = millis();
        while (cliKeepLoop())
        {
            if (millis() - pre_time >= 500)
            {
                pre_time = millis();

                if (color_index == 0)
                    cliPrintf("draw red\n");
                if (color_index == 1)
                    cliPrintf("draw green\n");
                if (color_index == 2)
                    cliPrintf("draw blue\n");

                pre_time_draw = micros();
                // st7735FillRect(0, 0, _width, _height, color_tbl[color_index]);
                color_index = (color_index + 1) % 3;

                cliPrintf("draw time : %d us, %d ms\n", micros() - pre_time_draw, (micros() - pre_time_draw) / 1000);
            }
            delay(1);
        }

        st7735SetCallBack(cb_func_pre);
        ret = true;
    }

    if (ret == false)
    {
        cliPrintf("st7789 info\n");
        cliPrintf("st7789 test\n");
    }
}

#endif
#endif