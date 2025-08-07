# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(AppDongle)

# QMK 모듈 CMake 파일을 포함합니다.
# include(src/ap/modules/qmk/CMakeLists.txt)


# 소스 파일들을 수집합니다. (비재귀적 방식)
# ap 및 bsp, hw 폴더의 모든 C/C++ 파일을 포함합니다.
file(GLOB SRC_FILES 
  src/ap/*.c
  src/bsp/*.c
  src/hw/driver/*.c
  src/hw/*.c
  src/hw/driver/usb/*.c
  src/hw/driver/usb/usb_hid/*.c
)

# 소스 파일들을 재귀적으로 수집합니다.
# common 및 hw 폴더 내의 모든 하위 디렉토리를 포함하여 C 파일을 찾습니다.
file(GLOB_RECURSE SRC_FILES_RECURSE
  src/common/*.c 
  # hw/*.c
)
# 소스 파일들을 app 타겟에 추가합니다
target_sources(app PRIVATE
  src/main.c
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
  # ${QMK_SRC_FILES}
)

# 프로젝트의 헤더 파일 검색 경로를 지정합니다.
# 컴파일 시 필요한 모든 헤더 파일이 있는 디렉토리를 PUBLIC으로 추가합니다.
target_include_directories(app PUBLIC
  src                   
  src/ap                     # 애플리케이션 관련 코드
  src/ap/modules             # 애플리케이션 모듈 디렉토리
  src/bsp                    # 보드 서포트 패키지(Board Support Package) 디렉토리
  src/common                 # 공통 코드 디렉토리
  src/common/core            # 코어 공통 기능
  src/common/hw/include      # 하드웨어 관련 헤더 파일
  src/hw                     # 하드웨어 드라이버 디렉토리
  src/hw/driver/usb          # USB 드라이버 디렉토리
  src/hw/driver/usb/usbd_hid # USB 드라이버 디렉토리
  src/lib                    # 라이브러리 디렉토리

  ${QMK_INC_DIR}         # QMK 헤더 파일 디렉토리
)


#[[
##########################################################################################################
# 파일 전역에서 .c 및 .cpp 파일들을 수집합니다
file(GLOB SRC_FILES 
  ap/*.c             # ap 디렉토리의 C 소스 파일들
  ap/*.cpp           # ap 디렉토리의 C++ 소스 파일들
)

# 지정된 디렉토리와 그 하위 디렉토리에서 재귀적으로 파일들을 수집합니다
file(GLOB_RECURSE SRC_FILES_RECURSE
  bsp/*.c            # bsp(Board Support Package) 디렉토리의 모든 C 파일
  common/*.c         # common 디렉토리의 모든 C 파일
  hw/*.c             # hw(Hardware) 디렉토리의 모든 C 파일
  hw/*.cpp;          # hw 디렉토리의 모든 C++ 파일 (세미콜론은 오타)
)

# 필요한 ESP-IDF 컴포넌트 목록을 설정합니다
set(requires 
  "spi_flash"        # SPI 플래시 메모리 관련 기능
  "esp_adc"          # ESP 아날로그-디지털 변환기 기능
  "esp_timer"        # ESP 타이머 기능
  "esp_psram"        # ESP PSRAM 관련 기능
  "vfs"              # 가상 파일 시스템
  "nvs_flash"        # Non-Volatile Storage 기능
  "esp_hid"          # ESP Human Interface Device 기능
  "esp_mm"           # ESP 메모리 관리 기능
  "tinyusb"          # TinyUSB 라이브러리
)

# ESP-IDF 컴포넌트 등록을 수행합니다
idf_component_register(
  SRCS "main.cpp"    # 메인 소스 파일
  
  SRCS ${SRC_FILES}  # 위에서 수집한 소스 파일들 추가
  SRCS ${SRC_FILES_RECURSE} # 재귀적으로 수집한 소스 파일들 추가
  SRCS ${QMK_SRC_FILES}     # QMK(Quantum Mechanical Keyboard) 관련 소스 파일

  # 포함 디렉토리 설정
  INCLUDE_DIRS ""                # 현재 디렉토리
  INCLUDE_DIRS "ap"              # 애플리케이션 코드 디렉토리
  INCLUDE_DIRS "ap/modules"      # 애플리케이션 모듈 디렉토리
  INCLUDE_DIRS "bsp"             # 보드 지원 패키지 디렉토리
  INCLUDE_DIRS "common"          # 공통 코드 디렉토리
  INCLUDE_DIRS "common/core"     # 공통 코어 코드 디렉토리
  INCLUDE_DIRS "common/hw/include" # 공통 하드웨어 헤더 디렉토리
  INCLUDE_DIRS "hw"              # 하드웨어 관련 디렉토리
  INCLUDE_DIRS "hw/driver"       # 하드웨어 드라이버 디렉토리
  INCLUDE_DIRS "hw/driver/usb"   # USB 드라이버 디렉토리
  INCLUDE_DIRS "hw/driver/ble"   # BLE 드라이버 디렉토리
  INCLUDE_DIRS "lib"             # 라이브러리 디렉토리

  INCLUDE_DIRS ${QMK_INC_DIR}    # QMK 헤더 디렉토리

  # 비공개 의존성 설정
  PRIV_REQUIRES soc driver freertos

  # 공개 의존성 설정
  REQUIRES "${requires}"         # 위에서 정의한 의존성 목록
  REQUIRES driver                # 드라이버 컴포넌트 추가
  )

  # 컴파일 옵션 설정 - 경고 메시지 제외
  target_compile_options(${COMPONENT_LIB} PRIVATE 
  "-Wno-format"                  # 포맷 관련 경고 비활성화
  "-Wno-expansion-to-defined"    # 매크로 확장 관련 경고 비활성화
  )

#######################################################################################################################

# QMK 모듈 CMake 파일을 포함합니다.
include(ap/modules/qmk/CMakeLists.txt)


# 소스 파일들을 수집합니다. (비재귀적 방식)
# ap 및 bsp, hw 폴더의 모든 C/C++ 파일을 포함합니다.
file(GLOB SRC_FILES 
  ap/*.cpp
  ap/*.c
  bsp/*.c
  hw/*.c
)

# 소스 파일들을 재귀적으로 수집합니다.
# common 및 hw 폴더 내의 모든 하위 디렉토리를 포함하여 C 파일을 찾습니다.
file(GLOB_RECURSE SRC_FILES_RECURSE
  common/*.c 
  hw/*.c
)

# 실행 파일을 추가합니다.
# 프로젝트 이름(${PRJ_NAME})으로 실행 파일을 생성하며, 필요한 모든 소스 파일을 포함합니다.
add_executable(
  ${PRJ_NAME}
  
  main.c

  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
  ${QMK_SRC_FILES}
)

# 프로젝트의 헤더 파일 검색 경로를 지정합니다.
# 컴파일 시 필요한 모든 헤더 파일이 있는 디렉토리를 PUBLIC으로 추가합니다.
target_include_directories(${PRJ_NAME} PUBLIC 
  .                      # 현재 디렉토리
  ap                     # 애플리케이션 관련 코드
  ap/modules             # 애플리케이션 모듈 디렉토리
  bsp                    # 보드 서포트 패키지(Board Support Package) 디렉토리
  common                 # 공통 코드 디렉토리
  common/core            # 코어 공통 기능
  common/hw/include      # 하드웨어 관련 헤더 파일
  hw                     # 하드웨어 드라이버 디렉토리
  hw/driver/usb          # USB 드라이버 디렉토리
  lib                    # 라이브러리 디렉토리

  ${QMK_INC_DIR}         # QMK 헤더 파일 디렉토리
)

# 프로젝트에 필요한 라이브러리를 링크합니다.
# Raspberry Pi Pico SDK의 컴포넌트들을 포함합니다.
target_link_libraries(${PRJ_NAME} 
  pico_stdlib           # Pico 표준 라이브러리
  hardware_spi          # SPI 통신 지원 하드웨어 라이브러리
  hardware_dma          # DMA(Direct Memory Access) 라이브러리
  hardware_irq          # IRQ(인터럽트 요청) 라이브러리
  hardware_i2c          # I2C 통신 지원 하드웨어 라이브러리
  pico_multicore        # 멀티코어 프로세싱 지원 라이브러리
  hardware_pio          # PIO(Programmable I/O) 하드웨어 라이브러리
  pico_unique_id        # Pico 보드 고유 식별자 라이브러리
  tinyusb_device        # TinyUSB 디바이스 라이브러리
  tinyusb_board         # TinyUSB 보드 지원 라이브러리
  )

# 추가적인 출력 파일을 생성합니다(UF2 파일 등).
pico_add_extra_outputs(${PRJ_NAME})
# USB를 통한 표준 입출력을 비활성화합니다.
pico_enable_stdio_usb(${PRJ_NAME}  0)
# UART를 통한 표준 입출력을 활성화합니다.
pico_enable_stdio_uart(${PRJ_NAME} 1)

# WS2812 LED 제어를 위한 PIO 헤더 파일을 생성합니다.
pico_generate_pio_header(${PRJ_NAME} ${CMAKE_CURRENT_LIST_DIR}/hw/driver/pio/ws2812.pio)

# 출력 파일의 확장자를 .elf로 설정합니다.
set_target_properties(${PRJ_NAME} PROPERTIES SUFFIX ".elf")

]]
