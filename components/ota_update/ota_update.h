#pragma once

#include "esp_err.h"

esp_err_t ota_perform_update(const char *firmware_url);
