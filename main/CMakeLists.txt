set(COMPONENT_SRCS
  "main.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
  "../include"
)

idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                       REQUIRES system_wrappers gpio_task wifi_task state_machine_task aws_iot_task file_system freertos)