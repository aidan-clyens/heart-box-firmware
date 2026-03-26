idf_component_register(SRCS
                        "test_application.c"
                        "test_app_main.c"
                        "test_aws_iot_task.c"
                        "test_generic_task.c"
                        "test_file_system.c"
                        "test_state_machine_task.c"
                        "test_wifi_task.c"
                        "test_gpio_task.c"
                        "test_gpio_interrupt.c"
                       INCLUDE_DIRS ""
                       INCLUDE_DIRS "../../include"
                       REQUIRES
                            unity
                            state_machine_task
                            aws_iot_task
                            gpio_task
                            wifi_task
                            file_system
                            system_wrappers
                       WHOLE_ARCHIVE)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  MQTT_CLIENT_IDENTIFIER="${DEVICE_NAME}"
  MQTT_TOPIC="${TOPIC_NAME}"
  MQTT_LOG_TOPIC="${LOG_TOPIC_NAME}"
  MQTT_BROKER_ENDPOINT="${AWS_IOT_ENDPOINT}"
  TEST_WIFI_SSID="${TEST_WIFI_SSID}"
  TEST_WIFI_PASSWORD="${TEST_WIFI_PASSWORD}"
)
