idf_component_register(
  SRCS "gpio_task.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos driver system_wrappers state_machine_task
)