cmake_minimum_required(VERSION 3.16.0)

set(AWS_IOT_PATH "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot")

set(EXTRA_COMPONENT_DIRS
    "${AWS_IOT_PATH}/libraries"
    "${AWS_IOT_PATH}/libraries/common/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
project(HeartBoxFirmware)

# Get config values from sdkconfig
idf_build_get_property(config_dir CONFIG_DIR)
include(${config_dir}/sdkconfig.cmake)

set(CERTS_PATH "${CMAKE_CURRENT_LIST_DIR}/certs/${CONFIG_HEARTBOX_DEVICE_NAME}")

add_definitions(-DDEVICE_NAME="${CONFIG_HEARTBOX_DEVICE_NAME}")
add_definitions(-DTOPIC_NAME="${CONFIG_HEARTBOX_TOPIC_NAME}")
add_definitions(-DAWS_IOT_ENDPOINT="${CONFIG_HEARTBOX_AWS_IOT_ENDPOINT}")

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
