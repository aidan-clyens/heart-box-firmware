#ifndef __MQTT_TASK_H__
#define __MQTT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

/** @enum eMqttMsgType_t
 *  @brief Type of commands and events for the MQTT task
 */
typedef enum
{
  MQTT_CMD_CONNECT,
  MQTT_CMD_DISCONNECT,
  MQTT_EVT_CONNECTED,
  MQTT_EVT_DISCONNECTED,
  MQTT_EVT_ERROR,
} eMqttMsgType_t;

/** @struct MqttMsg_t
 *  @brief Command or event message for the MQTT task
 */
typedef struct
{
  eMqttMsgType_t type;
  union
  {
    int error_code;
  } data;
} MqttMsg_t;

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