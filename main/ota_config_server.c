#include "ota_config_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "nvs_storage.h"
#include <string.h>
#include <sys/param.h>

static const char *TAG = "OTA_CFG";

// HTML del menú principal de configuración
static const char *CONFIG_MENU_HTML = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Configuración AllToPrint</title>"
"    <style>"
"        body {"
"            font-family: Arial, sans-serif;"
"            background-color: #f4f6f9;"
"            margin: 0;"
"            padding: 20px;"
"            display: flex;"
"            justify-content: center;"
"            align-items: center;"
"            min-height: 100vh;"
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
"        .menu-option {"
"            display: block;"
"            width: 100%;"
"            background: #007BFF;"
"            color: white;"
"            padding: 15px;"
"            margin: 10px 0;"
"            border: none;"
"            border-radius: 8px;"
"            font-size: 16px;"
"            cursor: pointer;"
"            text-decoration: none;"
"            text-align: center;"
"            transition: background 0.3s;"
"        }"
"        .menu-option:hover {"
"            background: #0056b3;"
"        }"
"        .info-box {"
"            background: #e7f3ff;"
"            padding: 15px;"
"            border-radius: 8px;"
"            margin: 20px 0;"
"            font-size: 14px;"
"            border-left: 4px solid #007BFF;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>⚙️ Configuración</h1>"
"        <div class='info-box'>"
"            <strong>Modo Configuración</strong><br>"
"            Selecciona una opción para configurar el sistema"
"        </div>"
"        <a href='/wifi_config' class='menu-option'>📡 Configurar WiFi</a>"
"        <a href='/ota_update' class='menu-option'>🔄 Actualizar Firmware</a>"
"    </div>"
"</body>"
"</html>";

// HTML de configuración WiFi
static const char *WIFI_CONFIG_HTML = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Configurar WiFi</title>"
"<style>"
"body{font-family:Arial,sans-serif;background:#f4f6f9;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}"
".container{width:90%;max-width:480px;background:#fff;padding:24px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.2)}"
"h1{text-align:center;color:#333;margin-bottom:20px}"
".mode-selector{display:flex;gap:10px;margin:20px 0}"
".mode-btn{flex:1;padding:12px;border:2px solid #007BFF;background:white;color:#007BFF;border-radius:8px;cursor:pointer;font-size:16px}"
".mode-btn.active{background:#007BFF;color:white}"
".config-section{display:none;margin:20px 0}"
".config-section.active{display:block}"
"label{display:block;margin:10px 0 5px;color:#333;font-weight:bold}"
"input[type=text],input[type=password]{width:100%;padding:12px;border:1px solid #ccc;border-radius:8px;box-sizing:border-box;font-size:14px}"
"button[type=submit]{width:100%;margin-top:20px;background:#28a745;color:white;padding:14px;font-size:18px;border:none;border-radius:8px;cursor:pointer}"
"button[type=submit]:hover{background:#218838}"
".back-link{display:block;text-align:center;margin-top:15px;color:#007BFF;text-decoration:none}"
".info{background:#fff3cd;padding:10px;border-radius:5px;margin:10px 0;font-size:13px;border-left:4px solid #ffc107}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>Configurar WiFi</h1>"
"<div class='mode-selector'>"
"<button type='button' class='mode-btn active' id='apBtn'>Modo AP</button>"
"<button type='button' class='mode-btn' id='staBtn'>Modo Station</button>"
"</div>"
"<form method='POST' action='/save_wifi'>"
"<input type='hidden' name='mode' id='modeInput' value='AP'>"
"<div id='apConfig' class='config-section active'>"
"<div class='info'><strong>Modo Access Point:</strong><br>El dispositivo crea su propia red WiFi (AllToPrint-XXXX)</div>"
"</div>"
"<div id='staConfig' class='config-section'>"
"<div class='info'><strong>Modo Station:</strong><br>El dispositivo se conecta a una red WiFi existente</div>"
"<label>SSID de la red:</label>"
"<input type='text' name='ssid' id='ssid' placeholder='Nombre de la red WiFi'>"
"<label>Contrasena:</label>"
"<input type='password' name='password' id='password' placeholder='Contrasena WiFi'>"
"</div>"
"<button type='submit'>Guardar y Reiniciar</button>"
"</form>"
"<a href='/' class='back-link'>Volver al menu</a>"
"</div>"
"<script>"
"var apBtn=document.getElementById('apBtn');"
"var staBtn=document.getElementById('staBtn');"
"var modeInput=document.getElementById('modeInput');"
"var apConfig=document.getElementById('apConfig');"
"var staConfig=document.getElementById('staConfig');"
"apBtn.onclick=function(){"
"modeInput.value='AP';"
"apBtn.className='mode-btn active';"
"staBtn.className='mode-btn';"
"apConfig.className='config-section active';"
"staConfig.className='config-section';"
"};"
"staBtn.onclick=function(){"
"modeInput.value='STA';"
"staBtn.className='mode-btn active';"
"apBtn.className='mode-btn';"
"staConfig.className='config-section active';"
"apConfig.className='config-section';"
"};"
"</script>"
"</body>"
"</html>";

// HTML de actualización OTA (sin cambios)
static const char *OTA_UPDATE_HTML = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Actualizar Firmware</title>"
"    <style>"
"        body {"
"            font-family: Arial, sans-serif;"
"            background-color: #f4f6f9;"
"            margin: 0;"
"            padding: 20px;"
"            display: flex;"
"            justify-content: center;"
"            align-items: center;"
"            min-height: 100vh;"
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
"        .info-box {"
"            background: #e7f3ff;"
"            padding: 15px;"
"            border-radius: 8px;"
"            margin: 20px 0;"
"            font-size: 14px;"
"            border-left: 4px solid #007BFF;"
"        }"
"        .file-input {"
"            width: 100%;"
"            padding: 12px;"
"            margin: 15px 0;"
"            border: 2px dashed #007BFF;"
"            border-radius: 8px;"
"            background: #f8f9fa;"
"            cursor: pointer;"
"            transition: all 0.3s ease;"
"            text-align: center;"
"        }"
"        .file-input:hover {"
"            background: #e7f3ff;"
"            border-color: #0056b3;"
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
"        .back-link {"
"            display: block;"
"            text-align: center;"
"            margin-top: 15px;"
"            color: #007BFF;"
"            text-decoration: none;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>🔄 Actualizar Firmware</h1>"
"        <div class='info-box'>"
"            <strong>Actualización OTA</strong><br>"
"            Sube un archivo .bin para actualizar el firmware"
"        </div>"
"        <form method='POST' action='/do_update' enctype='multipart/form-data' id='otaForm'>"
"            <label class='file-input'>"
"                📁 Seleccionar archivo .bin"
"                <input type='file' name='firmware' accept='.bin' required "
"                       style='display: none;' id='fileInput' onchange='updateFileName()'>"
"            </label>"
"            <div id='fileName' style='text-align: center; color: #666; font-size: 13px;'></div>"
"            <button type='submit' id='submitBtn'>Subir y Actualizar</button>"
"        </form>"
"        <a href='/' class='back-link'>← Volver al menú</a>"
"    </div>"
"    <script>"
"        function updateFileName() {"
"            const fileInput = document.getElementById('fileInput');"
"            const fileNameDiv = document.getElementById('fileName');"
"            if (fileInput.files.length > 0) {"
"                fileNameDiv.textContent = 'Archivo: ' + fileInput.files[0].name;"
"                fileNameDiv.style.color = '#007BFF';"
"            }"
"        }"
"        document.getElementById('otaForm').onsubmit = function() {"
"            document.getElementById('submitBtn').textContent = 'Subiendo...';"
"            document.getElementById('submitBtn').disabled = true;"
"        };"
"    </script>"
"</body>"
"</html>";

// Handler GET / (menú principal)
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, CONFIG_MENU_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler GET /wifi_config
static esp_err_t wifi_config_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WIFI_CONFIG_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler GET /ota_update
static esp_err_t ota_update_page_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, OTA_UPDATE_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler POST /save_wifi
static esp_err_t save_wifi_handler(httpd_req_t *req) {
    char buf[512] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(TAG, "Datos WiFi recibidos: %s", buf);
    
    // Parsear datos del formulario
    char mode[16] = {0};
    char ssid[64] = {0};
    char password[64] = {0};
    
    // Extraer modo
    char *mode_start = strstr(buf, "mode=");
    if (mode_start) {
        mode_start += 5;
        char *mode_end = strchr(mode_start, '&');
        int len = mode_end ? (mode_end - mode_start) : strlen(mode_start);
        strncpy(mode, mode_start, MIN(len, sizeof(mode) - 1));
    }
    
    // Si es modo STA, extraer SSID y contraseña
    if (strcmp(mode, "STA") == 0) {
        char *ssid_start = strstr(buf, "ssid=");
        if (ssid_start) {
            ssid_start += 5;
            char *ssid_end = strchr(ssid_start, '&');
            int len = ssid_end ? (ssid_end - ssid_start) : strlen(ssid_start);
            strncpy(ssid, ssid_start, MIN(len, sizeof(ssid) - 1));
        }
        
        char *pass_start = strstr(buf, "password=");
        if (pass_start) {
            pass_start += 9;
            char *pass_end = strchr(pass_start, '&');
            int len = pass_end ? (pass_end - pass_start) : strlen(pass_start);
            strncpy(password, pass_start, MIN(len, sizeof(password) - 1));
        }
        
        // Validar que haya SSID
        if (strlen(ssid) == 0) {
            httpd_resp_sendstr(req, 
                "<html><body><h1>Error: SSID requerido</h1>"
                "<a href='/wifi_config'>Volver</a></body></html>");
            return ESP_OK;
        }
    }
    
    // Guardar configuración en NVS
    esp_err_t err = nvs_set_str_value(WIFI_MODE_KEY, mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error guardando modo WiFi");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    if (strcmp(mode, "STA") == 0) {
        nvs_set_str_value(WIFI_SSID_KEY, ssid);
        nvs_set_str_value(WIFI_PASS_KEY, password);
        ESP_LOGI(TAG, "Configuración STA guardada: SSID=%s", ssid);
    } else {
        ESP_LOGI(TAG, "Configuración AP guardada");
    }
    
    // Respuesta de éxito
    httpd_resp_sendstr(req, 
        "<!DOCTYPE html>"
        "<html lang='es'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>Configuración Guardada</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; background: #f4f6f9; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }"
        ".container { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.2); text-align: center; }"
        "h1 { color: #28a745; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h1>✅ Configuración Guardada</h1>"
        "<p>El sistema se reiniciará en 3 segundos...</p>"
        "</div>"
        "</body>"
        "</html>");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

// Handler POST /do_update (sin cambios del original)
static esp_err_t ota_upload_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;
    uint8_t ota_buff[1024];
    int data_read;
    int total_received = 0;
    esp_err_t err = ESP_OK;
    bool ota_started = false;

    if (req == NULL) {
        ESP_LOGE(TAG, "Request NULL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== INICIANDO OTA UPDATE ===");
    
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No se encontró partición OTA válida");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin falló: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ota_started = true;

    int remaining = req->content_len;
    bool in_binary_section = false;

    while (remaining > 0) {
        int chunk_size = (remaining < sizeof(ota_buff)) ? remaining : sizeof(ota_buff);
        data_read = httpd_req_recv(req, (char*)ota_buff, chunk_size);
        
        if (data_read < 0) {
            if (data_read == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "Error de recepción");
            break;
        }
        
        if (data_read == 0) break;

        if (!in_binary_section) {
            for (int i = 0; i <= data_read - 4; i++) {
                if (ota_buff[i] == '\r' && ota_buff[i+1] == '\n' && 
                    ota_buff[i+2] == '\r' && ota_buff[i+3] == '\n') {
                    int binary_start = i + 4;
                    int binary_size = data_read - binary_start;
                    if (binary_size > 0) {
                        err = esp_ota_write(ota_handle, &ota_buff[binary_start], binary_size);
                        if (err != ESP_OK) break;
                        total_received += binary_size;
                        in_binary_section = true;
                    }
                    break;
                }
            }
        } else {
            err = esp_ota_write(ota_handle, ota_buff, data_read);
            if (err != ESP_OK) break;
            total_received += data_read;
        }

        remaining -= data_read;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (err != ESP_OK) goto ota_cleanup;

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) goto ota_cleanup;

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) goto ota_cleanup;

    ESP_LOGW(TAG, "OTA EXITOSA! Reiniciando...");
    httpd_resp_sendstr(req, "<html><body><h1>Actualización exitosa</h1><p>Reiniciando...</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;

ota_cleanup:
    if (ota_started && ota_handle != 0) {
        esp_ota_abort(ota_handle);
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

void register_ota_config_handlers(httpd_handle_t server) {
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t wifi_uri = {
        .uri = "/wifi_config",
        .method = HTTP_GET,
        .handler = wifi_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_uri);

    httpd_uri_t ota_page_uri = {
        .uri = "/ota_update",
        .method = HTTP_GET,
        .handler = ota_update_page_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_page_uri);

    httpd_uri_t save_wifi_uri = {
        .uri = "/save_wifi",
        .method = HTTP_POST,
        .handler = save_wifi_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &save_wifi_uri);

    httpd_uri_t ota_uri = {
        .uri = "/do_update",
        .method = HTTP_POST,
        .handler = ota_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_uri);
    
    ESP_LOGI(TAG, "Handlers de configuración registrados");
}