# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
set(AWS_IOT_PATH "${ROOT_PATH}/components/esp-aws-iot")

# Paths to components that will be built in the test_app.
# If the components have unit tests (in its test subdirectory) it will be run too.
set(EXTRA_COMPONENT_DIRS
  "${ROOT_PATH}/components"
  "${AWS_IOT_PATH}/libraries/coreMQTT"
  "${AWS_IOT_PATH}/libraries/common/"
  "${ROOT_PATH}/main"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(HeartBoxFirmwareTestApp)

# Get config values from sdkconfig
idf_build_get_property(config_dir CONFIG_DIR)
include(${config_dir}/sdkconfig.cmake)

set(CERTS_PATH "${ROOT_PATH}/certs/${CONFIG_HEARTBOX_DEVICE_NAME}")

add_definitions(-DDEVICE_NAME="${CONFIG_HEARTBOX_DEVICE_NAME}")
add_definitions(-DTOPIC_NAME="${CONFIG_HEARTBOX_TOPIC_NAME}")
add_definitions(-DAWS_IOT_ENDPOINT="${CONFIG_HEARTBOX_AWS_IOT_ENDPOINT}")
add_definitions(-DTEST_WIFI_SSID="${CONFIG_HEARTBOX_TEST_WIFI_SSID}")
add_definitions(-DTEST_WIFI_PASSWORD="${CONFIG_HEARTBOX_TEST_WIFI_PASSWORD}")

idf_build_set_property(MINIMAL_BUILD ON)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
