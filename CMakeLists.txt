cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(uwatch)

pico_sdk_init()

# LCD
add_subdirectory(./lib/lcd)
include_directories(./lib/lcd)

# IMU
add_subdirectory(./lib/QMI8658)
include_directories(./lib/QMI8658)

# Main
add_executable(main
  main.c
  utils.c
  lib/draw.c
)

# USB, UART
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(main)

# Ignore warning
set(GCC_COVERAGE_LINK_FLAGS "--no-warn-rwx-segments")

# Link
target_link_libraries(main
lcd QMI8658
pico_stdlib
hardware_spi
hardware_i2c
hardware_pwm
hardware_rtc
hardware_timer
hardware_adc
hardware_flash
hardware_sync
pico_bootsel_via_double_reset
)
