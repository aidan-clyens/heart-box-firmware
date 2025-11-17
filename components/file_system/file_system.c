#include "file_system.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "FILE SYSTEM";

static const char *NVS_NAMESPACE = "storage";

/** @brief Helper function to open NVS handle
 *  @param handle Pointer to nvs_handle_t to be opened
 *  @param mode NVS open mode (read-only or read-write)
*/
static void file_system_open_handle(nvs_handle_t *handle, nvs_open_mode_t mode)
{
  esp_err_t err = nvs_open(NVS_NAMESPACE, mode, handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
  }
}

/** @brief Public API: Initialize the file system (NVS) 
 *  @return ESP_OK on success, error code otherwise
 */
esp_err_t file_system_init(void)
{
  ESP_LOGI(TAG, "Initializing NVS");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
    ESP_LOGI(TAG, "NVS flash erased and reinitialized");
  }
  else if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "NVS initialized successfully");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
  }

  return ret;
}

void file_system_write_string(const char *key, const char *data)
{
  if (key == NULL || data == NULL)
  {
    ESP_LOGE(TAG, "Key or data is NULL");
    return;
  }

  ESP_LOGI(TAG, "Writing string to NVS. Key=%s, Data=%s", key, data);

  nvs_handle_t my_handle;
  esp_err_t err;

  file_system_open_handle(&my_handle, NVS_READWRITE);

  err = nvs_set_str(my_handle, key, data);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error writing string to NVS: %s", esp_err_to_name(err));
  }

  err = nvs_commit(my_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error committing changes to NVS: %s", esp_err_to_name(err));
  }

  nvs_close(my_handle);
}

char *file_system_read_string(const char *key)
{
  if (key == NULL)
  {
    ESP_LOGE(TAG, "Key is NULL");
    return NULL;
  }

  ESP_LOGI(TAG, "Reading string from NVS. Key=%s", key);

  nvs_handle_t my_handle;
  esp_err_t err;
  size_t required_size = 0;
  char *message = NULL;

  file_system_open_handle(&my_handle, NVS_READWRITE);

  err = nvs_get_str(my_handle, key, NULL, &required_size);
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to get size for key %s, error: %s", key, esp_err_to_name(err));
    nvs_close(my_handle);
    return NULL;
  }

  message = malloc(required_size);
  err = nvs_get_str(my_handle, key, message, &required_size);
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to read string for key %s, error: %s", key, esp_err_to_name(err));
    free(message);

    nvs_close(my_handle);
    return NULL;
  }

  ESP_LOGI(TAG, "Read string: %s", message);

  nvs_close(my_handle);
  return message;
}

void file_system_clear(const char *key)
{
  if (key == NULL)
  {
    ESP_LOGE(TAG, "Key is NULL");
    return;
  }

  nvs_handle_t my_handle;
  esp_err_t err;

  file_system_open_handle(&my_handle, NVS_READWRITE);

  err = nvs_erase_key(my_handle, key);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error erasing key %s from NVS: %s", key, esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAG, "Erased key %s from NVS", key);
  }

  nvs_close(my_handle);
}