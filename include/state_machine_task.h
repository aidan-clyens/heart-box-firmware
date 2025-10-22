#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

#include "message_types.h"

/** @brief Initialize and start the State Machine task */
void state_machine_task_init(void);

/** @brief Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppMsgType_t type, eAppMsgSource_t source);

#ifdef __cplusplus
}
#endif

#endif // __STATEMACHINE_TASK_H__