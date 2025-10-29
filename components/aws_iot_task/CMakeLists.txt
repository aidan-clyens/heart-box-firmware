idf_component_register(
  SRCS "aws_iot_task.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos system_wrappers state_machine_task coreMQTT posix_compat
)