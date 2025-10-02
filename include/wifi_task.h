#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SSID_LEN        32
#define MAX_PASSPHRASE_LEN  64

/** @enum eWifiCommand_t
 *  @brief Commands for the WiFi task
 */
typedef enum
{
  WIFI_CMD_MODE_AP,
  WIFI_CMD_MODE_AP_STA,
  WIFI_CMD_SET_STA_CREDENTIALS,
  WIFI_CMD_PING,
} eWifiCommand_t;

/** @struct WifiCredentials_t
 *  @brief Credentials to connect to a WiFi AP
 */
typedef struct
{
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASSPHRASE_LEN + 1];
} WifiCredentials_t;

/** @struct WifiCommandMsg_t
 *  @brief Command to send to the WiFi task with data
 */
typedef struct
{
  eWifiCommand_t cmd;
  union
  {
    WifiCredentials_t credentials;
  } data;
} WifiCommandMsg_t;

void wifi_task_init();
void wifi_post_cmd(WifiCommandMsg_t msg);

void wifi_set_ap_mode();
void wifi_set_sta_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__