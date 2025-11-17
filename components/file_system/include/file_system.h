#ifndef __FILE_SYSTEM_H__
#define __FILE_SYSTEM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_err.h"

/** @brief Public API: Initialize the file system (NVS) 
 *  @return ESP_OK on success, error code otherwise
 */
esp_err_t file_system_init(void);

void file_system_write_string(const char *key, const char *data);
char *file_system_read_string(const char *key);

void file_system_clear(const char *key);

#ifdef __cplusplus
}
#endif

#endif // __FILE_SYSTEM_H__