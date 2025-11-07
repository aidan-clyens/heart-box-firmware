# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(DEVICE_NAME "Heart_Box_1")
set(TOPIC_NAME "heartbox/1")
set(AWS_IOT_ENDPOINT "airgahux2exxu-ats.iot.us-east-1.amazonaws.com")

set(ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
set(AWS_IOT_PATH "${ROOT_PATH}/components/esp-aws-iot")
set(CERTS_PATH "${ROOT_PATH}/certs/${DEVICE_NAME}")

# Paths to components that will be built in the test_app.
# If the components have unit tests (in its test subdirectory) it will be run too.
# IMPORTANT: Must be set BEFORE project() call
set(EXTRA_COMPONENT_DIRS
  "${ROOT_PATH}/components"
  "${AWS_IOT_PATH}/libraries/coreMQTT"
  "${AWS_IOT_PATH}/libraries/common/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(HeartBoxFirmwareTestApp)

add_definitions(-DDEVICE_NAME="${DEVICE_NAME}")
add_definitions(-DTOPIC_NAME="${TOPIC_NAME}")
add_definitions(-DAWS_IOT_ENDPOINT="${AWS_IOT_ENDPOINT}")

idf_build_set_property(MINIMAL_BUILD ON)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
