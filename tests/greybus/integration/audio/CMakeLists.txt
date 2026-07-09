cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
project(fake_audio_test)

get_filename_component(GB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../.. ABSOLUTE)
target_include_directories(app PRIVATE 
  ${GB_ROOT}/drivers/audio
  ${GB_ROOT}/subsys/greybus
)

target_sources(app PRIVATE
  src/main.c 
  src/test_protocol.c
  ${GB_ROOT}/drivers/audio/fake_audio.c 
)
