#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

void wifi_task_init();

void wifi_set_sta_mode();
void wifi_set_ap_mode();

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__