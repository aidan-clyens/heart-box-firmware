#include "http_server.h"

#include "esp_log.h"

static const char *TAG = "http_server";

/** @brief Handler for the root URI
 * 
 *  @param req The HTTP request 
 *  @return esp_err_t ESP_OK on success
 */
static esp_err_t http_root_get_handler(httpd_req_t *req)
{
  const char resp[] = "Hello, world!";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_root_get_handler,
    .user_ctx = NULL};

/** @brief Start the HTTP web server
 *  @return httpd_handle_t The HTTP server handle
 */
httpd_handle_t http_start_webserver()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &root_uri);
  }
  else
  {
    ESP_LOGE(TAG, "Error starting server!");
  }
  return server;
}

/** @brief Stop the HTTP web server
 *  @param server The HTTP server handle 
 */
void http_stop_webserver(httpd_handle_t server)
{
  if (server)
  {
    httpd_stop(server);
  }
}