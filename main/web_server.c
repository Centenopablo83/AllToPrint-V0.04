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

// Endpoint de prueba
static esp_err_t test_get_handler(httpd_req_t *req) {
    httpd_resp_sendstr(req, "Servidor web funcionando!");
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    ESP_LOGI(TAG, "🔄 Iniciando servidor web...");

    if(httpd_start(&server, &config) == ESP_OK) {
        // Registrar endpoint general
        httpd_uri_t health = {
            .uri = "/health",
            .method = HTTP_GET,
            .handler = health_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &health);

        ESP_LOGI(TAG, "✅ Endpoint /health registrado");

        ESP_LOGI(TAG, "🔍 Buscando app activa...");

        httpd_uri_t test_uri = {
            .uri = "/test",
            .method = HTTP_GET,
            .handler = test_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &test_uri);
        ESP_LOGI(TAG, "✅ Endpoint /test registrado");

        // Delegar registro de endpoints específicos de la app
        const app_interface_t *app = get_active_app();


        if (app == NULL) {
            ESP_LOGE(TAG, "❌ get_active_app() retornó NULL");
        } else if (app->app_register_http_handlers == NULL) {
            ESP_LOGE(TAG, "❌ app_register_http_handlers es NULL");
        } else {
            ESP_LOGI(TAG, "✅ App encontrada, registrando handlers...");
            app->app_register_http_handlers(server);
            ESP_LOGI(TAG, "✅ Handlers de app registrados correctamente");
        }

        ESP_LOGI(TAG, "🚀 Servidor HTTP iniciado con endpoints de la app");
    } else {
        ESP_LOGE(TAG, "❌ No se pudo iniciar el servidor HTTP");
    }
    return server;
}