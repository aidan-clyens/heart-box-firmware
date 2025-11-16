# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
set(AWS_IOT_PATH "${ROOT_PATH}/components/esp-aws-iot")
set(CERTS_PATH "${ROOT_PATH}/certs/${DEVICE_NAME}")

# Validate certificate files exist before build
set(REQUIRED_CERT_FILES
  "${CERTS_PATH}/AmazonRootCA1.pem"
  "${CERTS_PATH}/device_certificate.pem.crt"
  "${CERTS_PATH}/private_key.pem.key"
)

foreach(CERT_FILE ${REQUIRED_CERT_FILES})
  if(NOT EXISTS ${CERT_FILE})
    message(FATAL_ERROR "Certificate file not found: ${CERT_FILE}\n"
                        "Device: ${DEVICE_NAME}\n"
                        "Certificates directory: ${CERTS_PATH}\n"
                        "Please ensure certificates are placed in the correct directory.")
  endif()
endforeach()

message(STATUS "Certificate validation passed for device: ${DEVICE_NAME}")
message(STATUS "Using certificates from: ${CERTS_PATH}")

# Paths to components that will be built in the test_app.
# If the components have unit tests (in its test subdirectory) it will be run too.
set(EXTRA_COMPONENT_DIRS
  "${ROOT_PATH}/components"
  "${AWS_IOT_PATH}/libraries/coreMQTT"
  "${AWS_IOT_PATH}/libraries/common/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(HeartBoxFirmwareTestApp)

idf_build_set_property(MINIMAL_BUILD ON)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/AmazonRootCA1.pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/device_certificate.pem.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CERTS_PATH}/private_key.pem.key" TEXT)
