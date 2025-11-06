#pragma once
#include "esp_http_server.h"

typedef struct {
    void (*app_init)(void);
    void (*app_handle_message)(const char *msg);
    void (*app_register_http_handlers)(httpd_handle_t server);
    const char *(*app_get_html)(void);
} app_interface_t;

const app_interface_t *get_active_app(void);