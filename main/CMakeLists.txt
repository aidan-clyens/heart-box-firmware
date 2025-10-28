set(COMPONENT_SRCS
  "main.c"
  "gpio_task.c"
  "wifi_task.c"
  "state_machine_task.c"
  "http_server.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
  "../include"
)

idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                       REQUIRES system-wrappers coreMQTT esp_http_server nvs_flash esp_wifi esp_event esp_system freertos driver)