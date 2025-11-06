#include "app_interface.h"
#include "printer_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "APP_PREGUNTAS";

const char *html_form = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Pregunta lo que quieras!!</title>"
"    <style>"
"        body {"
"            font-family: Arial, sans-serif;"
"            background-color: #f4f6f9;"
"            margin: 0;"
"            padding: 0;"
"            display: flex;"
"            justify-content: center;"
"            align-items: center;"
"            height: 100vh;"
"        }"
"        .container {"
"            width: 90%;"
"            max-width: 480px;"
"            background: #fff;"
"            padding: 24px;"
"            border-radius: 12px;"
"            box-shadow: 0 4px 12px rgba(0,0,0,0.2);"
"        }"
"        h1 {"
"            text-align: center;"
"            color: #333;"
"            margin-bottom: 20px;"
"        }"
"        textarea {"
"            width: 100%;"
"            height: 120px;"
"            padding: 12px;"
"            font-size: 16px;"
"            border: 1px solid #ccc;"
"            border-radius: 8px;"
"            box-sizing: border-box;"
"            resize: none;"
"        }"
"        button {"
"            margin-top: 20px;"
"            width: 100%;"
"            background-color: #007BFF;"
"            color: white;"
"            padding: 14px;"
"            font-size: 18px;"
"            border: none;"
"            border-radius: 8px;"
"            cursor: pointer;"
"            transition: background-color 0.3s ease;"
"        }"
"        button:hover {"
"            background-color: #0056b3;"
"        }"
"        .success {"
"            text-align: center;"
"            color: green;"
"            font-size: 16px;"
"            margin-top: 12px;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>Escribí lo que quieras</h1>"
"        <form action='/msg' method='POST'>"
"            <textarea name='msg' placeholder='Escribe tu mensaje ANONIMO aquí...' required></textarea>"
"            <button type='submit'>Enviar</button>"
"        </form>"
"    </div>"
"</body>"
"</html>";

static void app_init(void) {
    printer_init();
    ESP_LOGI(TAG, "App 'Preguntas' inicializada");
}

static void app_handle_message(const char *msg) {
    ESP_LOGI(TAG, "Mensaje recibido: %s", msg);
    printer_send_text(msg);
}

// Handler POST /msg
static esp_err_t msg_post_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Procesar mensaje recibido
    app_handle_message(buf);

    // Responder OK al cliente
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Handler GET /
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Registrar endpoints de la app
static void app_register_http_handlers(httpd_handle_t server) {
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t msg_uri = {
        .uri = "/msg",
        .method = HTTP_POST,
        .handler = msg_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &msg_uri);
}

// Función específica para esta app
const app_interface_t *get_app_preguntas(void) {
    static const app_interface_t app = {
        .app_init = app_init,
        .app_handle_message = app_handle_message,
        .app_register_http_handlers = app_register_http_handlers,
        .app_get_html = NULL
    };
    return &app;
}