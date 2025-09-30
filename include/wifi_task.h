#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  WIFI_CMD_MODE_AP_STA,
  WIFI_CMD_SET_STA_CREDENTIALS,
  WIFI_CMD_PING,
} eWifiCommand_t;

typedef enum
{
  WIFI_EVENT_CONNECTED,
  WIFI_EVENT_DISCONNECTED,
} eWifiEvent_t;

void wifi_task_init();
void wifi_post_cmd(eWifiCommand_t cmd);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__