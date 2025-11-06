#include "ota_config_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include <string.h>
#include <sys/param.h>

static const char *TAG = "OTA_UPLOAD";

// HTML MEJORADO con el mismo estilo de las apps
static const char *OTA_CONFIG_HTML = 
"<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Actualizar Aplicación</title>"
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
"        .instructions {"
"            font-size: 12px;"
"            color: #666;"
"            text-align: center;"
"            margin-top: 15px;"
"        }"
"        .status {"
"            text-align: center;"
"            margin-top: 15px;"
"            padding: 10px;"
"            border-radius: 5px;"
"            display: none;"
"        }"
"        .success {"
"            background: #d4edda;"
"            color: #155724;"
"            border: 1px solid #c3e6cb;"
"        }"
"        .error {"
"            background: #f8d7da;"
"            color: #721c24;"
"            border: 1px solid #f5c6cb;"
"        }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>Actualizar Aplicación</h1>"
"        <div class='info-box'>"
"            <strong>Modo Configuración OTA</strong><br>"
"            Sube un archivo .bin para cambiar la aplicación del sistema"
"        </div>"
"        <form method='POST' action='/do_update' enctype='multipart/form-data' id='otaForm'>"
"            <label class='file-input'>"
"                📁 Seleccionar archivo .bin"
"                <input type='file' name='firmware' accept='.bin' required "
"                       style='display: none;' id='fileInput' onchange='updateFileName()'>"
"            </label>"
"            <div id='fileName' class='instructions'></div>"
"            <button type='submit' id='submitBtn'>Subir y Actualizar</button>"
"        </form>"
"        <div class='instructions'>"
"            El sistema se reiniciará automáticamente después de la actualización"
"        </div>"
"        <div id='statusMessage' class='status'></div>"
"    </div>"
"    <script>"
"        function updateFileName() {"
"            const fileInput = document.getElementById('fileInput');"
"            const fileNameDiv = document.getElementById('fileName');"
"            if (fileInput.files.length > 0) {"
"                fileNameDiv.textContent = 'Archivo seleccionado: ' + fileInput.files[0].name;"
"                fileNameDiv.style.color = '#007BFF';"
"            } else {"
"                fileNameDiv.textContent = '';"
"            }"
"        }"
"        "
"        document.getElementById('otaForm').onsubmit = function() {"
"            const submitBtn = document.getElementById('submitBtn');"
"            submitBtn.disabled = true;"
"            submitBtn.textContent = 'Subiendo...';"
"            submitBtn.style.backgroundColor = '#6c757d';"
"        };"
"    </script>"
"</body>"
"</html>";

// Handler GET /
static esp_err_t root_ota_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, OTA_CONFIG_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 🔥 VERSIÓN SEGURA - Handler POST /do_update
static esp_err_t ota_upload_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;
    uint8_t ota_buff[1024];  // Buffer estático - más seguro
    int data_read;
    int total_received = 0;
    esp_err_t err = ESP_OK;
    bool ota_started = false;

    // 🔥 VERIFICACIÓN DE SEGURIDAD: Validar parámetros
    if (req == NULL) {
        ESP_LOGE(TAG, "Request NULL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== INICIANDO OTA UPDATE ===");
    ESP_LOGI(TAG, "Tamaño total: %d bytes", req->content_len);

    // 1. Obtener partición de actualización CON VERIFICACIÓN
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No se encontró partición OTA válida");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 🔥 VERIFICACIÓN: Log seguro sin accesos a punteros problemáticos
    ESP_LOGI(TAG, "Partición OTA encontrada: tipo=%d, subtype=%d", 
             update_partition->type, update_partition->subtype);

    // 2. Iniciar operación OTA CON VERIFICACIÓN
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin falló: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ota_started = true;
    ESP_LOGI(TAG, "OTA begin exitoso");

    // 3. Procesar datos en chunks pequeños y SEGUROS
    int remaining = req->content_len;
    bool in_binary_section = false;
    int skip_counter = 0;
    const int MAX_SKIP = 4096; // Máximo bytes a buscar boundary

    ESP_LOGI(TAG, "Comenzando recepción de datos...");

    while (remaining > 0 && total_received < (10 * 1024 * 1024)) { // Límite de 10MB por seguridad
        // Leer chunk pequeño
        int chunk_size = (remaining < sizeof(ota_buff)) ? remaining : sizeof(ota_buff);
        data_read = httpd_req_recv(req, (char*)ota_buff, chunk_size);
        
        if (data_read < 0) {
            if (data_read == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Timeout, reintentando...");
                continue;
            }
            ESP_LOGE(TAG, "Error de recepción: %d", data_read);
            break;
        }
        
        if (data_read == 0) {
            ESP_LOGW(TAG, "Conexión cerrada por cliente");
            break;
        }

        // 🔥 PROCESAMIENTO SEGURO
        if (!in_binary_section) {
            // Buscar el inicio de los datos binarios de forma SEGURA
            bool found_boundary = false;
            int binary_start_offset = 0;
            
            for (int i = 0; i <= data_read - 4; i++) {
                if (ota_buff[i] == '\r' && ota_buff[i+1] == '\n' && 
                    ota_buff[i+2] == '\r' && ota_buff[i+3] == '\n') {
                    found_boundary = true;
                    binary_start_offset = i + 4;
                    break;
                }
            }
            
            if (found_boundary && binary_start_offset < data_read) {
                int binary_size = data_read - binary_start_offset;
                if (binary_size > 0) {
                    err = esp_ota_write(ota_handle, &ota_buff[binary_start_offset], binary_size);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Error en esp_ota_write: %s", esp_err_to_name(err));
                        break;
                    }
                    total_received += binary_size;
                    in_binary_section = true;
                    ESP_LOGI(TAG, "Datos binarios encontrados después de %d bytes", skip_counter);
                }
            } else {
                // Seguir buscando boundary
                skip_counter += data_read;
                if (skip_counter > MAX_SKIP) {
                    ESP_LOGW(TAG, "Límite de búsqueda alcanzado, escribiendo todo como binario");
                    in_binary_section = true;
                    err = esp_ota_write(ota_handle, ota_buff, data_read);
                    if (err != ESP_OK) break;
                    total_received += data_read;
                }
            }
        } else {
            // Ya estamos en sección binaria - escribir directamente
            err = esp_ota_write(ota_handle, ota_buff, data_read);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error en esp_ota_write: %s", esp_err_to_name(err));
                break;
            }
            total_received += data_read;
        }

        remaining -= data_read;

        // Log de progreso SEGURO (sin divisiones por cero)
        if (req->content_len > 0 && (total_received % (100 * 1024) == 0)) {
            int progress = (total_received * 100) / req->content_len;
            ESP_LOGI(TAG, "Progreso: %d/%d bytes (%d%%)", 
                    total_received, req->content_len, progress);
        }
        
        // Pequeña pausa para evitar sobrecarga
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // 🔥 VERIFICAR SI HUBO ERROR DURANTE EL PROCESO
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error durante procesamiento OTA");
        goto ota_cleanup;
    }

    // 4. Finalizar OTA
    ESP_LOGI(TAG, "Finalizando OTA, %d bytes recibidos", total_received);
    err = esp_ota_end(ota_handle);
    ota_handle = 0; // Ya no usamos el handle
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end falló: %s", esp_err_to_name(err));
        goto ota_cleanup;
    }

    // 5. Configurar nueva partición de arranque
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition falló: %s", esp_err_to_name(err));
        goto ota_cleanup;
    }

    // 6. ÉXITO - Reiniciar
    ESP_LOGW(TAG, "🎉 OTA EXITOSA! %d bytes procesados. Reiniciando...", total_received);
    
    // 🔥 MEJORADO: Página de éxito con el mismo estilo
    httpd_resp_sendstr(req, 
        "<!DOCTYPE html>"
        "<html lang='es'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>Actualización Exitosa</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; background: #f4f6f9; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }"
        ".container { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.2); max-width: 480px; text-align: center; }"
        "h1 { color: #28a745; margin-bottom: 20px; }"
        "p { color: #333; margin: 10px 0; }"
        ".loader { border: 4px solid #f3f3f3; border-top: 4px solid #007BFF; border-radius: 50%; width: 40px; height: 40px; animation: spin 2s linear infinite; margin: 20px auto; }"
        "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h1>✅ Actualización Exitosa</h1>"
        "<p>La aplicación se ha actualizado correctamente.</p>"
        "<p>El sistema se reiniciará en 3 segundos...</p>"
        "<div class='loader'></div>"
        "</div>"
        "</body>"
        "</html>");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

    return ESP_OK;

ota_cleanup:
    if (ota_started && ota_handle != 0) {
        esp_ota_abort(ota_handle);
    }
    ESP_LOGE(TAG, "OTA falló después de %d bytes", total_received);
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

void register_ota_config_handlers(httpd_handle_t server) {
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_ota_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t ota_uri = {
        .uri = "/do_update",
        .method = HTTP_POST,
        .handler = ota_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_uri);
    
    ESP_LOGI(TAG, "Handlers OTA registrados");
}