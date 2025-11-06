#include "app_interface.h"
#include "printer_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "APP_VOTACION";

const char *html_votacion = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Votación Anónima</title>"
"    <style>"
"        * {"
"            box-sizing: border-box;"
"        }"
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
"        .opcion {"
"            display: block;"
"            width: 100%;"
"            background-color: #f8f9fa;"
"            border: 2px solid #dee2e6;"
"            border-radius: 8px;"
"            padding: 15px;"
"            margin: 10px 0;"
"            font-size: 18px;"
"            cursor: pointer;"
"            transition: all 0.3s ease;"
"            text-align: center;"
"            box-sizing: border-box;"
"        }"
"        .opcion:hover {"
"            background-color: #007BFF;"
"            color: white;"
"            border-color: #007BFF;"
"        }"
"        .selected {"
"            background-color: #28a745;"
"            color: white;"
"            border-color: #28a745;"
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
"            box-sizing: border-box;"
"        }"
"        button:hover {"
"            background-color: #0056b3;"
"        }"
"        button:disabled {"
"            background-color: #cccccc;"
"            cursor: not-allowed;"
"        }"
"        .success {"
"            text-align: center;"
"            color: green;"
"            font-size: 16px;"
"            margin-top: 12px;"
"            display: none;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>Votación Anónima</h1>"
"        <form action='/vote' method='POST'>"
"            <div class='opcion' onclick='selectOption(this)' data-value='opcion1'>Opción 1</div>"
"            <div class='opcion' onclick='selectOption(this)' data-value='opcion2'>Opción 2</div>"
"            <div class='opcion' onclick='selectOption(this)' data-value='opcion3'>Opción 3</div>"
"            <input type='hidden' name='voto' id='votoInput'>"
"            <button type='submit' id='submitBtn' disabled>Enviar Voto</button>"
"        </form>"
"        <div class='success' id='successMsg'>¡Voto enviado correctamente!</div>"
"        <script>"
"            function selectOption(element) {"
"                document.querySelectorAll('.opcion').forEach(el => el.classList.remove('selected'));"
"                element.classList.add('selected');"
"                document.getElementById('votoInput').value = element.getAttribute('data-value');"
"                document.getElementById('submitBtn').disabled = false;"
"            }"
"            "
"            document.querySelector('form').addEventListener('submit', function(e) {"
"                e.preventDefault();"
"                if(document.getElementById('votoInput').value) {"
"                    document.getElementById('submitBtn').disabled = true;"
"                    document.getElementById('submitBtn').textContent = 'Enviando...';"
"                    "
"                    setTimeout(function() {"
"                        document.getElementById('successMsg').style.display = 'block';"
"                        document.querySelector('form').style.display = 'none';"
"                    }, 1000);"
"                    "
"                    // Aquí se enviaría el formulario realmente"
"                    // this.submit();"
"                }"
"            });"
"        </script>"
"    </div>"
"</body>"
"</html>";

static void app_init(void) {
    printer_init();
    ESP_LOGI(TAG, "App 'Votación' inicializada");
}

static void app_handle_message(const char *msg) {
    ESP_LOGI(TAG, "Voto recibido: %s", msg);
    char print_buffer[256];
    snprintf(print_buffer, sizeof(print_buffer), "VOTO: %s", msg);
    printer_send_text(print_buffer);
}

// Handler POST /vote
static esp_err_t vote_post_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Extraer el voto (formato: voto=opcion1)
    char *voto_start = strstr(buf, "voto=");
    if (voto_start) {
        voto_start += 5; // Saltar "voto="
        app_handle_message(voto_start);
    } else {
        app_handle_message(buf);
    }

    httpd_resp_sendstr(req, "Voto registrado!");
    return ESP_OK;
}

// Handler GET /
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_votacion, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void app_register_http_handlers(httpd_handle_t server) {
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t vote_uri = {
        .uri = "/vote",
        .method = HTTP_POST,
        .handler = vote_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &vote_uri);
}

const app_interface_t *get_app_votacion(void) {
    static const app_interface_t app = {
        .app_init = app_init,
        .app_handle_message = app_handle_message,
        .app_register_http_handlers = app_register_http_handlers,
        .app_get_html = NULL
    };
    return &app;
}