#pragma once
#include "esp_err.h"


esp_err_t printer_init(void);
esp_err_t printer_send_text(const char *text);