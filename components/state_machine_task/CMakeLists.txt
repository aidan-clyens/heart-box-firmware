idf_component_register(
  SRCS "state_machine_task.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos system_wrappers gpio_task wifi_task aws_iot_task file_system
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  MQTT_BROKER_ENDPOINT="${AWS_IOT_ENDPOINT}"
  MQTT_TOPIC="${AWS_IOT_TOPIC}"
)