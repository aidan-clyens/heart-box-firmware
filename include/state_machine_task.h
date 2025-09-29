#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
  STATE_STARTING,
  STATE_REGISRATION,
  STATE_INIT,
  STATE_RUNNING,
} AppState_t;

typedef enum
{
  EVENT_NO_WIFI_CREDENTIALS,
  EVENT_WIFI_CREDENTIALS,
  EVENT_WIFI_CONNECTED,
  EVENT_WIFI_DISCONNECTED,
} AppEvent_t;

void state_machine_create_task();

#ifdef __cplusplus
}
#endif

#endif // __STATEMACHINE_TASK_H__