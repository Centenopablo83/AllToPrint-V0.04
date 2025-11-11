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
"        .error { background: #f8d7da; color: #721c24; }"
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
"        <div id='status' class='status waiting'>Conectando...</div>"
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
"        let consecutiveErrors = 0;"
"        "
"        textarea.addEventListener('input', function() {"
"            charCount.textContent = this.value.length;"
"        });"
"        "
"        function checkPrinterStatus() {"
"            console.log('🔍 Consultando /printer_status...');"
"            fetch('/printer_status', {"
"                method: 'GET',"
"                cache: 'no-cache'"
"            })"
"            .then(response => {"
"                console.log('📡 Response status:', response.status, response.ok);"
"                if (!response.ok) {"
"                    throw new Error('HTTP error ' + response.status);"
"                }"
"                return response.text();"
"            })"
"            .then(text => {"
"                console.log('📄 Raw response:', text);"
"                try {"
"                    const data = JSON.parse(text);"
"                    console.log('📊 Parsed data:', data);"
"                    consecutiveErrors = 0;"
"                    "
"                    if (data.ready === true) {"
"                        statusDiv.textContent = '✓ Impresora lista';"
"                        statusDiv.className = 'status ready';"
"                        submitBtn.disabled = false;"
"                        console.log('✅ Impresora LISTA');"
"                    } else {"
"                        statusDiv.textContent = '⏳ Esperando impresora...';"
"                        statusDiv.className = 'status waiting';"
"                        submitBtn.disabled = true;"
"                        console.log('⏳ Impresora NO lista');"
"                    }"
"                } catch(e) {"
"                    console.error('❌ JSON parse error:', e);"
"                    statusDiv.textContent = '✗ Error de formato';"
"                    statusDiv.className = 'status error';"
"                }"
"            })"
"            .catch((error) => {"
"                consecutiveErrors++;"
"                console.error('💥 Fetch error:', error);"
"                statusDiv.textContent = '✗ Error de conexión (' + consecutiveErrors + ')';"
"                statusDiv.className = 'status error';"
"                submitBtn.disabled = true;"
"            });"
"        }"
"        "
"        console.log('🚀 Iniciando monitor...');"
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
"                    checkPrinterStatus();"
"                }, 2000);"
"            });"
"        });"
"    </script>"
"</body>"
"</html>";

static void app_init(void) {
    // NO inicializar printer aquí - ya se hizo en main.c
    pregunta_counter = 0;
    ESP_LOGI(TAG, "App 'Preguntas' inicializada (printer ya iniciado en main)");
}

static void imprimir_pregunta(const char *texto) {
    if (!printer_is_ready()) {
        ESP_LOGW(TAG, "Impresora no lista, mensaje no impreso");
        return;
    }
        
    char print_buffer[512];
    int offset = 0;
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%s",
                      ESC_ALIGN_CENTER,
                      "PREGUNTA ANONIMA\n");
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s",
                      "================================\n");
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s\n\n",
                      texto);
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s================================\n",
                      ESC_ALIGN_CENTER);
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "AllToPrint - Preguntas\n");
    
    offset += snprintf(print_buffer + offset, sizeof(print_buffer) - offset,
                      "%s%s",
                      ESC_FEED_3,
                      ESC_CUT_PARTIAL);
    
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

static esp_err_t msg_post_handler(httpd_req_t *req) {
    char buf[512] = {0}; 
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    char *msg_start = NULL;
    char *msg_end = NULL;

    // 1. Encontrar el inicio del campo 'msg'
    char *msg_header = strstr(buf, "name=\"msg\""); 
    
    if (msg_header) {
        // 2. Buscar el doble salto de línea ESTÁNDAR HTTP (CRLF)
        msg_start = strstr(msg_header, "\r\n\r\n");
        
        // Fallback: si no es el estándar (aunque el JS debería usar CRLF), probamos con solo \n\n
        if (!msg_start) {
            msg_start = strstr(msg_header, "\n\n");
        }

        if (msg_start) {
            // Ajustar el puntero para que apunte al inicio del contenido real
            // Si encontramos \r\n\r\n, avanzamos 4 bytes. Si encontramos \n\n, avanzamos 2.
            msg_start += (msg_start[1] == '\n') ? 2 : 4; 
            
            // 3. Encontrar el final del contenido (el boundary). El boundary comienza con \r\n
            msg_end = strstr(msg_start, "\r\n------"); 
            
            // Fallback: si no es el estándar, probar con solo \n
            if (!msg_end) {
                msg_end = strstr(msg_start, "\n------");
            }
            
            // 4. Terminar el string
            if (msg_end) {
                // El caracter antes del boundary debe ser el terminador de línea del mensaje. Lo reemplazamos con '\0'.
                *msg_end = '\0'; 
                
                // Limpiar espacios en blanco o saltos de línea al final del mensaje extraído
                size_t len = strlen(msg_start);
                // Si el mensaje termina con \r o \n, lo eliminamos
                while (len > 0 && (msg_start[len - 1] == '\r' || msg_start[len - 1] == '\n' || msg_start[len - 1] == ' ')) {
                    msg_start[len - 1] = '\0';
                    len--;
                }

                // Llamar al manejador con el mensaje limpio
                app_handle_message(msg_start);
            } else {
                // Caso extremo o si es el único campo sin boundary claro (lo enviamos "crudo")
                app_handle_message(msg_start);
            }
        } else {
            ESP_LOGE(TAG, "Error: Contenido del mensaje no delimitado correctamente.");
            app_handle_message("Error de formato multipart"); 
        }
    } else {
        // Fallback: Si no es multipart/form-data (p. ej., si es form-urlencoded), usar el buffer crudo
        // Nota: Si usas este fallback, deberías re-introducir la lógica de decodificación de URL de tu código original.
        ESP_LOGW(TAG, "No se encontró 'name=\"msg\"'. Procesando como URL-encoded o texto plano.");
        app_handle_message(buf);
    }
    
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 🔥 HANDLER MEJORADO CON MÁS LOGS
static esp_err_t printer_status_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "📞 /printer_status consultado");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    bool ready = printer_is_ready();
    
    ESP_LOGI(TAG, "📊 printer_is_ready() = %s", ready ? "TRUE" : "FALSE");
    
    char response[128];
    int len = snprintf(response, sizeof(response), 
                      "{\"ready\":%s,\"counter\":%lu}", 
                      ready ? "true" : "false",
                      pregunta_counter);
    
    ESP_LOGI(TAG, "📤 Enviando: %s", response);
    
    esp_err_t ret = httpd_resp_send(req, response, len);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error enviando respuesta: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ Respuesta enviada correctamente");
    }
    
    return ret;
}

static void app_register_http_handlers(httpd_handle_t server) {
    ESP_LOGI(TAG, "📝 Registrando endpoints HTTP...");
    
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };
    esp_err_t ret = httpd_register_uri_handler(server, &root_uri);
    ESP_LOGI(TAG, "/ → %s", esp_err_to_name(ret));

    httpd_uri_t msg_uri = {
        .uri = "/msg",
        .method = HTTP_POST,
        .handler = msg_post_handler,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &msg_uri);
    ESP_LOGI(TAG, "/msg → %s", esp_err_to_name(ret));
    
    httpd_uri_t status_uri = {
        .uri = "/printer_status",
        .method = HTTP_GET,
        .handler = printer_status_handler,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &status_uri);
    ESP_LOGI(TAG, "/printer_status → %s", esp_err_to_name(ret));
    
    ESP_LOGI(TAG, "🎯 Endpoints registrados");
}

const app_interface_t *get_app_preguntas(void) {
    static const app_interface_t app = {
        .app_init = app_init,
        .app_handle_message = app_handle_message,
        .app_register_http_handlers = app_register_http_handlers,
        .app_get_html = NULL
    };
    
    static bool initialized = false;
    if (!initialized) {
        app_init();
        initialized = true;
        ESP_LOGI(TAG, "🏁 App Preguntas inicializada");
    }
    
    return &app;
}

const app_interface_t *get_active_app(void) {
    return get_app_preguntas();
}