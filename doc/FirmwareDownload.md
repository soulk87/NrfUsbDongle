# 다운로드 방법

## 개요

- Bootloader 는 Adafruit nRF52 bootloader 를 사용
- nice_nano_bootloader.hex 파일을 UF2 bootloader 로 사용
- Dongle, Right, Left 모두 동일한 bootloader 사용

## UF2 bootloader

- 초기 Bootloader 다운로드 방법 (Jlink 필요)

```bash
nrfjprog -e
nrfjprog --program nice_nano_bootloader.hex
```

## Bootloader 빌드 방법

- <https://github.com/adafruit/Adafruit_nRF52_Bootloader> <https://blog.naver.com/chcbaram/223705280470>를 참고해서 hex 파일을 만들 수 있다.
- git 에 있는 것을 다운로드 하여 사용

## UF2 Application 빌드 방법

- boards/arm/nicenanov2/nicenanov2_defconfig 파일에 아래 옵션활성화

```kconfig
CONFIG_BUILD_OUTPUT_UF2=y
CONFIG_ROM_START_OFFSET=0x1000
```

- CONFIG_BUILD_OUTPUT_UF2 를 사용하면 UF2 파일이 `build/zephyr/zephyr.uf2` 로 생성됨.
- CONFIG_ROM_START_OFFSET 으로 bootloader 가 생성될 자리를 만들어줌.
