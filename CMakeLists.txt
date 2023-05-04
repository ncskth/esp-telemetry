cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults")
project(c3_telemetry)
