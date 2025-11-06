#pragma once
#include "esp_http_server.h"

// Función que se llamará desde main.c en modo AP_Config
void register_ota_config_handlers(httpd_handle_t server);

// 🔥 NUEVO: Función para servidor OTA mejorado
httpd_handle_t start_ota_server(void);