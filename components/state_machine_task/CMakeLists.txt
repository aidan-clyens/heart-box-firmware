idf_component_register(
  SRCS "state_machine_task.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos system_wrappers gpio_task wifi_task
)