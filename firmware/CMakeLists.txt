cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD none CACHE STRING "Board")
set(PICO_PLATFORM rp2350 CACHE STRING "Platform")
set(PICO_FLASH_SIZE_BYTES 2097152 CACHE STRING "RP2354B in-package flash")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(beef_board_rp2354b C CXX ASM)
pico_sdk_init()

add_executable(beef_board_rp2354b
  src/main.c
  src/beef_runtime.c
  src/beef_lighting.c
  src/phoenixwan_uart.c
  src/beef_protocol.c
  src/beef_config_store.c
  src/usb_descriptors.c
)

pico_generate_pio_header(beef_board_rp2354b ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)
target_include_directories(beef_board_rp2354b PRIVATE src)
target_compile_definitions(beef_board_rp2354b PRIVATE
  PICO_RP2350A=0
  PICO_FLASH_SIZE_BYTES=2097152
)
target_link_libraries(beef_board_rp2354b PRIVATE
  pico_stdlib
  pico_unique_id
  pico_bootrom
  pico_flash
  hardware_flash
  hardware_watchdog
  hardware_dma
  hardware_pio
  hardware_uart
  hardware_timer
  tinyusb_device
  tinyusb_board
)
pico_enable_stdio_usb(beef_board_rp2354b 0)
pico_enable_stdio_uart(beef_board_rp2354b 0)
# Reserve two configuration sectors; retain the physical 2 MiB size for flash IO.
pico_add_linker_script_override_path(beef_board_rp2354b
  ${CMAKE_CURRENT_LIST_DIR}/ld FILES pico_flash_region.ld)
pico_set_program_name(beef_board_rp2354b "Beef Board RP2354B")
pico_set_program_version(beef_board_rp2354b "1.00")
pico_add_extra_outputs(beef_board_rp2354b)
