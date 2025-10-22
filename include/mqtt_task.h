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

/** @brief Request a disconnect from the MQTT broker */
void mqtt_disconnect(void);

/** @brief Post a message to the MQTT task
 *  @param msg The message to post
 *  @return pdTRUE if posted successfully, errQUEUE_FULL otherwise
 */
BaseType_t mqtt_post_msg(MqttMsg_t msg);

#ifdef __cplusplus
}
#endif

#endif // __MQTT_TASK_H__