#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SSID_LEN        32
#define MAX_PASSPHRASE_LEN  64

typedef int BaseType_t;

/** @enum eWifiMsgType_t
 *  @brief Commands for the WiFi task
 */
typedef enum
{
  WIFI_MSG_NONE = -1,
  WIFI_CMD_MODE_AP,
  WIFI_CMD_MODE_AP_STA,
  WIFI_CMD_SET_STA_CREDENTIALS,
  WIFI_CMD_PING,
  WIFI_EVT_STA_START,
  WIFI_EVT_STA_DISCONNECTED,
  WIFI_EVT_AP_START,
  WIFI_EVT_AP_STOP,
  WIFI_EVT_STA_GOT_IP,
} eWifiMsgType_t;

/** @struct WifiCredentials_t
 *  @brief Credentials to connect to a WiFi AP
 */
typedef struct
{
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASSPHRASE_LEN + 1];
} WifiCredentials_t;

/** @struct WifiMsg_t
 *  @brief Command to send to the WiFi task with data
 */
typedef struct
{
  eWifiMsgType_t type;
  union
  {
    WifiCredentials_t credentials;
  } data;
} WifiMsg_t;

void wifi_task_init();
BaseType_t wifi_post_msg(WifiMsg_t msg);

void wifi_set_ap_mode();
void wifi_set_sta_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__