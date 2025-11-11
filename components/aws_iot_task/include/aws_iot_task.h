#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

/** @brief Initialize and start the AWS IoT task */
void aws_iot_task_init(void);

void aws_iot_task_stop(void);

bool aws_iot_task_is_running(void);

/** @brief Request a connection to the configured AWS IoT broker */
void aws_iot_connect(void);

/** @brief Start listening for incoming MQTT messages from AWS IoT */
void aws_iot_start_listening(void);

/** @brief Publish a button press event to AWS IoT */
void aws_iot_publish_button_pressed_event(void);

/** @brief Publish a button released event to AWS IoT */
void aws_iot_publish_button_released_event(void);

#ifdef __cplusplus
}
#endif

#endif // __AWS_IOT_TASK_H__