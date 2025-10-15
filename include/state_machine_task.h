#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef int BaseType_t;

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

void state_machine_task_init();
BaseType_t state_machine_post_event(eAppEvent_t event);

#ifdef __cplusplus
}
#endif

#endif // __STATEMACHINE_TASK_H__