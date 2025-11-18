cmake_minimum_required(VERSION 3.16.0)

set(AWS_IOT_PATH "${CMAKE_CURRENT_LIST_DIR}/../components/esp-aws-iot")

set(EXTRA_COMPONENT_DIRS
    "${AWS_IOT_PATH}/libraries"
    "${AWS_IOT_PATH}/libraries/common/"
    "${CMAKE_CURRENT_LIST_DIR}/../components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
project(HeartBoxFirmwareTests)

# Get config values from sdkconfig
idf_build_get_property(config_dir CONFIG_DIR)
include(${config_dir}/sdkconfig.cmake)

set(CONFIG_HEARTBOX_DEVICE_NAME "Test")
set(CERTS_PATH "${CMAKE_CURRENT_LIST_DIR}/../certs/${CONFIG_HEARTBOX_DEVICE_NAME}")

# Validate certificate files exist before build
set(REQUIRED_CERT_FILES
  "${CERTS_PATH}/AmazonRootCA1.pem"
  "${CERTS_PATH}/device_certificate.pem.crt"
  "${CERTS_PATH}/private_key.pem.key"
)

foreach(CERT_FILE ${REQUIRED_CERT_FILES})
  if(NOT EXISTS ${CERT_FILE})
    message(FATAL_ERROR "Certificate file not found: ${CERT_FILE}\n"
                        "Device: ${CONFIG_HEARTBOX_DEVICE_NAME}\n"
                        "Certificates directory: ${CERTS_PATH}\n"
                        "Please ensure certificates are placed in the correct directory.")
  endif()
endforeach()

message(STATUS "Certificate validation passed for device: ${CONFIG_HEARTBOX_DEVICE_NAME}")
message(STATUS "Using certificates from: ${CERTS_PATH}")

add_definitions(-DDEVICE_NAME="${CONFIG_HEARTBOX_DEVICE_NAME}")
add_definitions(-DTOPIC_NAME="${CONFIG_HEARTBOX_TOPIC_NAME}")
add_definitions(-DAWS_IOT_ENDPOINT="${CONFIG_HEARTBOX_AWS_IOT_ENDPOINT}")

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
