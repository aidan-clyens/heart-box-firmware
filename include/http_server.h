#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

httpd_handle_t http_start_webserver();
void http_stop_webserver(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // __HTTP_SERVER_H__