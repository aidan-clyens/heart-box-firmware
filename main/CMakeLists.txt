set(COMPONENT_SRCS
  "main.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
  "../include"
)

idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                       REQUIRES system_wrappers gpio_task wifi_task state_machine_task coreMQTT esp_http_server nvs_flash esp_wifi esp_event esp_system freertos driver)