#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_err.h"

esp_err_t application_init(void);

esp_err_t application_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // __APPLICATION_H__
