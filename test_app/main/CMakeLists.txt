idf_component_register(SRCS "test_app_main.c" "test_file_system.c" "test_state_machine_task.c"
                       INCLUDE_DIRS ""
                       REQUIRES
                            unity
                            state_machine_task
                            aws_iot_task
                            gpio_task
                            wifi_task
                            file_system
                            system_wrappers
                       WHOLE_ARCHIVE)