#include "web_server.h"
#include "esp_log.h"
#include "app_interface.h"

static const char *TAG = "HTTP";

// Endpoint general /health
static esp_err_t health_get_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK) {
        // Registrar endpoint general
        httpd_uri_t health = {
            .uri = "/health",
            .method = HTTP_GET,
            .handler = health_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &health);

        // Delegar registro de endpoints específicos de la app
        const app_interface_t *app = get_active_app();
        if(app && app->app_register_http_handlers) {
            app->app_register_http_handlers(server);
        }

        ESP_LOGI(TAG, "Servidor HTTP iniciado con endpoints de la app");
    } else {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor HTTP");
    }
    return server;
}