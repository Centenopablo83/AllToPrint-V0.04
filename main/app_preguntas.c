#include "app_interface.h"
#include "printer_driver.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "APP_PREGUNTAS";
static uint32_t pregunta_counter = 0;

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
"        .status {"
"            text-align: center;"
"            padding: 10px;"
"            border-radius: 5px;"
"            margin-bottom: 15px;"
"        }"
"        .ready { background: #d4edda; color: #155724; }"
"        .waiting { background: #fff3cd; color: #856404; }"
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
"        .char-counter {"
"            text-align: right;"
"            font-size: 12px;"
"            color: #666;"
"            margin-top: 5px;"
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
"        button:disabled {"
"            background-color: #cccccc;"
"            cursor: not-allowed;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>Escribí lo que quieras</h1>"
"        <div id='status' class='status waiting'>Esperando impresora...</div>"
"        <form action='/msg' method='POST' id='msgForm'>"
"            <textarea name='msg' id='msgText' placeholder='Escribe tu mensaje ANÓNIMO aquí...' "
"                     maxlength='200' required></textarea>"
"            <div class='char-counter'><span id='charCount'>0</span>/200</div>"
"            <button type='submit' id='submitBtn' disabled>Enviar</button>"
"        </form>"
"    </div>"
"    <script>"
"        const textarea = document.getElementById('msgText');"
"        const charCount = document.getElementById('charCount');"
"        const submitBtn = document.getElementById('submitBtn');"
"        const statusDiv = document.getElementById('status');"
"        "
"        textarea.addEventListener('input', function() {"
"            charCount.textContent = this.value.length;"
"        });"
"        "
"        // Verificar estado de impresora cada 2 segundos"
"        function checkPrinterStatus() {"
"            fetch('/printer_status')"
"                .then(r => r.json())"
"                .then(data => {"
"                    if (data.ready) {"
"                        statusDiv.textContent = '✓ Impresora lista';"
"                        statusDiv.className = 'status ready';"
"                        submitBtn.disabled = false;"
"                    } else {"
"                        statusDiv.textContent = '⏳ Esperando impresora...';"
"                        statusDiv.className = 'status waiting';"
"                        submitBtn.disabled = true;"
"                    }"
"                })"
"                .catch(() => {"
"                    statusDiv.textContent = '✗ Error de conexión';"
"                    statusDiv.className = 'status waiting';"
"                });"
"        }"
"        "
"        checkPrinterStatus();"
"        setInterval(checkPrinterStatus, 2000);"
"        "
"        document.getElementById('msgForm').addEventListener('submit', function(e) {"
"            e.preventDefault();"
"            const formData = new FormData(this);"
"            submitBtn.disabled = true;"
"            submitBtn.textContent = 'Enviando...';"
"            "
"            fetch('/msg', {"
"                method: 'POST',"
"                body: formData"
"            }).then(r => r.text()).then(() => {"
"                textarea.value = '';"
"                charCount.textContent = '0';"
"                submitBtn.textContent = '✓ Enviado!';"
"                setTimeout(() => {"
"                    submitBtn.textContent = 'Enviar';"
"                    submitBtn.disabled = false;"
"                }, 2000);"
"            });"
"        });"
"    </script>"
"</body>"
"</html>";

static void app_init(void) {
    // El printer_init() ya se llamó en main.c
    pregunta_counter = 0;
    ESP_LOGI(TAG, "App 'Preguntas' inicializada");
}

// Función para formatear e imprimir pregunta
static void imprimir_pregunta(const char *texto) {
    if (!printer_is_ready()) {
        ESP_LOGW(TAG, "Impresora no lista, mensaje no impreso");
        return;
    }
    
    pregunta_counter++;
    
    // Obtener timestamp
    time_t now;
    struct tm timeinfo;
    char timestamp[20];
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%d/%m %H:%M", &timeinfo);
    
    // Buffer para el trabajo de impresión completo
    char print_buffer[512];
    int offset = 0;
    
    // 1. Encabezado con línea separadora
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%s%s",
                      ESC_ALIGN_CENTER,
                      ESC_DOUBLE_SIZE_ON,
                      "PREGUNTA ANONIMA\n");
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%s",
                      ESC_NORMAL_SIZE,
                      "================================\n\n");
    
    // 2. Número y timestamp
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%sNro: %lu\n",
                      ESC_ALIGN_LEFT,
                      ESC_BOLD_ON,
                      pregunta_counter);
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "Fecha: %s%s\n\n",
                      timestamp,
                      ESC_BOLD_OFF);
    
    // 3. Texto de la pregunta
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s\n\n",
                      texto);
    
    // 4. Pie y corte
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s================================\n",
                      ESC_ALIGN_CENTER);
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "AllToPrint - Preguntas\n");
    
    // Avanzar 3 líneas y corte parcial
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%s",
                      ESC_FEED_3,
                      ESC_CUT_PARTIAL);
    
    // Enviar todo de una vez
    esp_err_t ret = printer_send_raw((uint8_t*)print_buffer, offset);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Pregunta #%lu encolada para impresión", pregunta_counter);
    } else {
        ESP_LOGE(TAG, "✗ Error encolando pregunta: %s", esp_err_to_name(ret));
    }
}

static void app_handle_message(const char *msg) {
    ESP_LOGI(TAG, "Mensaje recibido: %s", msg);
    imprimir_pregunta(msg);
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
    
    // Decodificar URL (msg=texto+con+espacios)
    char decoded[256] = {0};
    char *msg_start = strstr(buf, "msg=");
    if (msg_start) {
        msg_start += 4; // Saltar "msg="
        
        // Decodificación simple de URL
        int j = 0;
        for (int i = 0; msg_start[i] && j < sizeof(decoded)-1; i++) {
            if (msg_start[i] == '+') {
                decoded[j++] = ' ';
            } else if (msg_start[i] == '%' && msg_start[i+1] && msg_start[i+2]) {
                // Decodificar %XX
                char hex[3] = {msg_start[i+1], msg_start[i+2], 0};
                decoded[j++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else if (msg_start[i] == '&') {
                break; // Fin del valor
            } else {
                decoded[j++] = msg_start[i];
            }
        }
        decoded[j] = '\0';
        
        app_handle_message(decoded);
    } else {
        app_handle_message(buf);
    }
    
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Handler GET / (página principal)
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler GET /printer_status (nuevo - para JS)
static esp_err_t printer_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    bool ready = printer_is_ready();
    ESP_LOGI(TAG, "Estado impresora consultado: %s", ready ? "LISTA" : "NO LISTA");
    
    char response[64];
    snprintf(response, sizeof(response), 
             "{\"ready\":%s}", 
             ready ? "true" : "false");
    
    httpd_resp_sendstr(req, response);
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
    
    httpd_uri_t status_uri = {
        .uri = "/printer_status",
        .method = HTTP_GET,
        .handler = printer_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);
}

const app_interface_t *get_app_preguntas(void) {
    static const app_interface_t app = {
        .app_init = app_init,
        .app_handle_message = app_handle_message,
        .app_register_http_handlers = app_register_http_handlers,
        .app_get_html = NULL
    };
    return &app;
}