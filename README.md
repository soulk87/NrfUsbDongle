# Cygnus Orb

## 주요기능

- 무선 Cygnus 키보드
- RF 2.44GHz 통신
- VIA 지원
- KEYMAP DISPLAY

## Hardware

- **MCU BOARD** : NrfMicro ([Alternatives](https://github.com/joric/nrfmicro/wiki/Alternatives))
- **LCD** : ST7789 1.3 inch (240x240)

### PinMap

#### Dongle

- **J1**

| J1  | PIN NAME | FUNCTION        | 비고         |
|-----|----------|-----------------|--------------|
| 1   | GND      |                 |              |
| 2   | P0.06    | UART_TX         | DEBUG        |
| 3   | P0.08    | UART_RX         | DEBUG        |
| 4   | GND      |                 |              |
| 5   | GND      |                 |              |
| 6   | P0.17    |                 |              |
| 7   | P0.20    |                 |              |
| 8   | P0.22    | BLK(ST7789)     |              |
| 9   | P0.24    | SCL(ST7789)     |              |
| 10  | P1.00    | SDA(ST7789)     |              |
| 11  | P0.11    | RES(ST7789)     |              |
| 12  | P1.04    | DC(ST7789)      |              |
| 13  | P1.06    | DC(ST7789)      |              |

- **J2** : 연결 안함

#### Right

- **J1**

| J1  | PIN NAME | FUNCTION        | 비고         |
|-----|----------|-----------------|--------------|
| 1   | GND      |                 |              |
| 2   | P0.06    | UART_TX         |    DEBUG     |
| 3   | P0.08    | UART_RX         |    DEBUG     |
| 4   | GND      |                 |              |
| 5   | GND      |                 |              |
| 6   | P0.17    |                 |              |
| 7   | P0.20    |                 |              |
| 8   | P0.22    |                 |              |
| 9   | P0.24    | SDIO(PMW3610)   |              |
| 10  | P1.00    | SCK(PMW3610)    |              |
| 11  | P0.11    | CS(PMW3610)     |              |
| 12  | P1.04    | RST(PMW3610)    |              |
| 13  | P1.06    | MOTION(PMW3610) |              |

- **J2**

| J2  | PIN NAME | FUNCTION        | 비고                     |
|-----|----------|-----------------|--------------------------|
| 1   | BAT+     |                 |                          |
| 2   | BAT+     |                 | bat                      |
| 3   | GND      |                 |                          |
| 4   | RESET    |                 | rst                      |
| 5   | 3.3V     |                 | P0.13이 LOW 인 경우 ON    |
| 6   | P0.31    | Col 1           |                          |
| 7   | P0.29    | Col 2           |                          |
| 8   | P0.02    | Col 3           |                          |
| 9   | P1.15    | Col 4           |                          |
| 10  | P1.13    | Col 5           |                          |
| 11  | P1.11    | Col 6           |                          |
| 12  | P0.10    | Row 1           |                          |
| 13  | P0.09    | Row 2           |                          |

- **J3**

| J3  | PIN NAME | FUNCTION        | 비고         |
|-----|----------|-----------------|--------------|
| 1   | P1.01    | Row 3           |              |
| 2   | P1.02    | Row 4           |              |
| 3   | P1.07    |                 |              |

#### Left

- Right 에서 PMW3610 제외하고 동일. (마우스 센서 없음)

## 🙏 Thanks to

- **cygnus** - [GitHub Repository](https://github.com/juhakaup/keyboards)
- **chcbaram** - [GitHub Repository](https://github.com/chcbaram)
- **QMK Firmware** - [GitHub Repository](https://github.com/qmk/qmk_firmware)
- **lvgl** - [GitHub Repository](https://github.com/lvgl/lvgl)

## ⚠️참고 사항

- 개발 진행 중이므로 일부 기능이 미완성일 수 있습니다.