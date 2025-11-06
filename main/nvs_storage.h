#pragma once
#include "esp_err.h"
#include <stddef.h>


void nvs_storage_init(void);
esp_err_t nvs_set_str_value(const char *key, const char *value);
esp_err_t nvs_get_str_value(const char *key, char *out, size_t outlen, const char *default_value);