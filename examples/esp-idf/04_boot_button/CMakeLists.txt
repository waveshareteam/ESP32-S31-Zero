cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s31" CACHE STRING "ESP-IDF target")
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(04_boot_button)
