#include "wifi_http_server.h"

#include "esp_log.h"
#include <string.h>

#include "wifi_task.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char * TAG_HTTP = "HTTP";

/** @brief Handler for the root URI
 * 
 *  @param req The HTTP request 
 *  @return esp_err_t ESP_OK on success
 */
static esp_err_t http_root_get_handler(httpd_req_t *req)
{
  const char resp[] = " \
    <html> \
      <head><title>Heart Box Login</title></head> \
      <body> \
        <h1>Enter your Wifi SSID and Password</h1> \
        <div> \
          <form action=\"/wifi\" method=\"post\"> \
            <label for=\"ssid\">SSID:</label><br> \
            <input type=\"text\" id=\"ssid\" name=\"ssid\"><br> \
            <label for=\"password\">Password:</label><br> \
            <input type=\"password\" id=\"password\" name=\"password\"><br><br> \
            <input type=\"submit\" value=\"Submit\"> \
          </form> \
      </body> \
    </html>";

  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_root_get_handler,
    .user_ctx = NULL};

/** @brief Handler for the /wifi POST URI
 *
 *  @param req The HTTP request
 *  @return esp_err_t ESP_OK on success
 */
static esp_err_t http_wifi_post_handler(httpd_req_t *req)
{
  char buf[128];
  int ret, remaining = req->content_len;

  // Read the request body
  int received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
  if (received <= 0)
  {
    if (received == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  buf[received] = '\0'; // null terminate

  ESP_LOGI(TAG_HTTP, "Received POST data: %s", buf);

  // Example form body: "ssid=MyNetwork&password=Secret123"
  char ssid[32] = {0};
  char password[64] = {0};

  if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK)
  {
    ESP_LOGI(TAG_HTTP, "SSID: %s", ssid);
  }
  if (httpd_query_key_value(buf, "password", password, sizeof(password)) == ESP_OK)
  {
    ESP_LOGI(TAG_HTTP, "Password: %s", password);
  }

  ESP_LOGI(TAG_HTTP, "Parsed SSID: %s", ssid);
  ESP_LOGI(TAG_HTTP, "Parsed Password: %s", password);

  wifi_set_sta_credentials(ssid, password);

  // Send response back to client
  const char resp[] = "<html><body><h2>Credentials received. Connecting...</h2></body></html>";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
}

static const httpd_uri_t wifi_uri = {
    .uri = "/wifi",
    .method = HTTP_POST,
    .handler = http_wifi_post_handler,
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
    httpd_register_uri_handler(server, &wifi_uri);

    ESP_LOGI(TAG_HTTP, "Started HTTP server");
  }
  else
  {
    ESP_LOGE(TAG_HTTP, "Failed to start HTTP server");
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
    if (httpd_stop(server) != ESP_OK)
    {
      ESP_LOGE(TAG_HTTP, "Failed to stop HTTP server");
    }
    else
    {
      ESP_LOGI(TAG_HTTP, "Stopped HTTP server");
    }
  }
}