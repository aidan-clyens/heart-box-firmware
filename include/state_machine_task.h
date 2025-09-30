#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
  APP_EVENT_WIFI_CONNECTED,
} eAppEvent_t;

void state_machine_task_init();
void state_machine_post_event(eAppEvent_t event);

#ifdef __cplusplus
}
#endif

#endif // __STATEMACHINE_TASK_H__