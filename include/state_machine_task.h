#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

/** @enum eAppEvent_t
 *  @brief Events for the State Machine task
 */
typedef enum
{
  APP_EVENT_WIFI_CONNECTED,
  APP_EVENT_WIFI_DISCONNECTED,
  APP_EVENT_AP_STARTED,
  APP_EVENT_BUTTON_PRESSED,
} eAppEvent_t;

/** @struct StateMachineMsg_t
 *  @brief Message type for the State Machine task
 */
typedef struct
{
  eAppEvent_t event;
} StateMachineMsg_t;

/** @brief Initialize and start the State Machine task */
void state_machine_task_init(void);

/** @brief Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppEvent_t event);

#ifdef __cplusplus
}
#endif

#endif // __STATEMACHINE_TASK_H__