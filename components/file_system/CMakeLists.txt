idf_component_register(
  SRCS "file_system.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos system_wrappers nvs_flash
)