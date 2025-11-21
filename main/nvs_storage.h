#pragma once
#include "esp_err.h"
#include <stddef.h>

// Keys NVS
#define BOOT_MODE_KEY       "boot_mode"
#define WIFI_MODE_KEY       "wifi_mode"
#define WIFI_SSID_KEY       "wifi_ssid"
#define WIFI_PASS_KEY       "wifi_pass"

// Valores de modo
#define BOOT_MODE_CONFIG    "AP_config"
#define BOOT_MODE_NORMAL    "NORMAL"
#define WIFI_MODE_AP        "AP"
#define WIFI_MODE_STA       "STA"

void nvs_storage_init(void);
esp_err_t nvs_set_str_value(const char *key, const char *value);
esp_err_t nvs_get_str_value(const char *key, char *out, size_t outlen, const char *default_value);