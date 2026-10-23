cmake_minimum_required(VERSION 3.20.0)

# Out-of-tree board: hantek_2d82 lives in ./boards/hantek_2d82/ (symlink to
# vendors/hantek/2d82/reference/dso2000/zephyr/hantek_2d82). BOARD_ROOT = app dir so
# Zephyr finds boards/<name>/board.yml here.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Out-of-tree snippet: mcuboot-hantek_2d82 (Option C flash layout) lives in
# ./snippets/. SNIPPET_ROOT = app dir so Zephyr/sysbuild finds it without
# needing to be added to any west manifest. See
# snippets/mcuboot-hantek_2d82/snippet.yml and vendors/hantek/2d82/docs/firmware/flash-layout.md.
list(APPEND SNIPPET_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../a2k_board)
list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
# awto_board excluded: contains ESP32-specific headers (soc/rtc_cntl_reg.h) that break STM32 build.
# lib/ is a Zephyr module (lib/zephyr/module.yml) — builds pure C + STM32 + Zephyr glue in one pass.

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(a2k_first_stage VERSION 0.1 LANGUAGES C)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/screens/screen_placeholder.c
    src/screens/screen_generic.c
    src/screens/screen_boot.c
    src/screens/screen_waveform.c
    src/screens/screen_dmm.c
    src/screens/screen_awg.c
    src/screens/screen_menu.c
    src/screens/screen_cal.c
)
