cmake_minimum_required(VERSION 3.16.0)

set(DEVICE_NAME "Test")
set(TOPIC_NAME "")
set(AWS_IOT_ENDPOINT "")

set(AWS_IOT_PATH "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot")
set(CERTS_PATH "${CMAKE_CURRENT_LIST_DIR}/certs/${DEVICE_NAME}")

set(EXTRA_COMPONENT_DIRS
    "${AWS_IOT_PATH}/libraries"
    "${AWS_IOT_PATH}/libraries/common/"
)

add_definitions(-DDEVICE_NAME="${DEVICE_NAME}")
add_definitions(-DTOPIC_NAME="${TOPIC_NAME}")
add_definitions(-DAWS_IOT_ENDPOINT="${AWS_IOT_ENDPOINT}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
project(HeartBoxFirmware)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
