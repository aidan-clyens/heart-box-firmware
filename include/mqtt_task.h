#ifndef __MQTT_TASK_H__
#define __MQTT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

#include "message_types.h"

/** @brief Initialize and start the MQTT task */
void mqtt_task_init(void);

/** @brief Request a connection to the configured MQTT broker */
void mqtt_connect(void);

#ifdef __cplusplus
}
#endif

#endif // __MQTT_TASK_H__