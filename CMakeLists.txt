cmake_minimum_required(VERSION 3.16.0)

set(AWS_IOT_PATH "components/esp-aws-iot")

set(EXTRA_COMPONENT_DIRS
    "${AWS_IOT_PATH}/libraries"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
project(HeartBoxFirmware)
