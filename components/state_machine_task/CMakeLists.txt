idf_component_register(
  SRCS "state_machine_task.c"
  INCLUDE_DIRS "include"
  REQUIRES freertos system_wrappers gpio_task wifi_task aws_iot_task file_system
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  MQTT_BROKER_ENDPOINT="${CONFIG_HEARTBOX_AWS_IOT_ENDPOINT}"
  MQTT_TOPIC="${CONFIG_HEARTBOX_TOPIC_NAME}"
)