# UF2 로 다운로드 설정 방법

## UF2 bootloader

* nrfjprog --program nice_nano_bootloader.hex
* <https://github.com/adafruit/Adafruit_nRF52_Bootloader> <https://blog.naver.com/chcbaram/223705280470>를 참고해서 hex 파일을 만들 수 있다.
* git 에 있는 것을 다운로드 하여 사용

## APP 설정

```kconfig
CONFIG_BUILD_OUTPUT_UF2=y
CONFIG_ROM_START_OFFSET=0x1000
```

* CONFIG_BUILD_OUTPUT_UF2 를 사용하면 UF2 파일이 생성됨.
* CONFIG_ROM_START_OFFSET 으로 bootloader 가 생성될 자리를 만들어줌.
