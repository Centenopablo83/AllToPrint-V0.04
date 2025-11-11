#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "nvs_storage.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "ota_config_server.h"
#include "printer_driver.h"  // 🔥 AGREGADO

#define BUTTON_GPIO         GPIO_NUM_0
#define BUTTON_HOLD_TIME_MS 5000
#define BOOT_MODE_KEY       "boot_mode"
#define BOOT_MODE_CONFIG    "AP_config"
#define BOOT_MODE_NORMAL    "NORMAL"

static const char *TAG = "MAIN";
static httpd_handle_t server = NULL;

static void button_monitor_task(void *pvParameters);
static bool is_button_pressed(void);

static bool is_button_pressed(void) {
    return gpio_get_level(BUTTON_GPIO) == 0;
}

static void button_monitor_task(void *pvParameters) {
    while (1) {
        if (is_button_pressed()) {
            int64_t start = esp_timer_get_time();
            ESP_LOGI(TAG, "Detectado BOOT presionado, esperando %d ms...", BUTTON_HOLD_TIME_MS);

            while (is_button_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(50));
                int64_t now = esp_timer_get_time();
                if ((now - start) / 1000 >= BUTTON_HOLD_TIME_MS) {
                    char current_mode[16] = {0};
                    nvs_get_str_value(BOOT_MODE_KEY, current_mode, sizeof(current_mode), BOOT_MODE_CONFIG);

                    const char *new_mode = 
                        (strcmp(current_mode, BOOT_MODE_CONFIG) == 0) ? BOOT_MODE_NORMAL : BOOT_MODE_CONFIG;

                    ESP_LOGW(TAG, "Botón mantenido %.1f s → cambiando modo a: %s",
                             (double)(now - start) / 1e6, new_mode);

                    nvs_set_str_value(BOOT_MODE_KEY, new_mode);

                    ESP_LOGW(TAG, "Reiniciando para aplicar cambios...");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Función para modo CONFIGURACIÓN (solo OTA)
static void start_config_mode(void) {
    ESP_LOGI(TAG, "=== MODO CONFIGURACIÓN ===");
    
    wifi_manager_init_ap();
    
    // 🔥 CONFIGURACIÓN MEJORADA con más stack
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 24576;  // 🔥 24KB de stack (en lugar de 4KB)
    config.max_uri_handlers = 16;
    config.max_resp_headers = 16;
    config.recv_wait_timeout = 30;
    config.send_wait_timeout = 30;
    config.lru_purge_enable = true;
    
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        register_ota_config_handlers(server);
        ESP_LOGI(TAG, "Servidor OTA MEJORADO listo en http://192.168.4.1");
    } else {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor OTA");
    }
}

// Función para modo NORMAL (app actual)
static void start_normal_mode(void) {
    ESP_LOGI(TAG, "=== MODO NORMAL ===");
    
    // Configurar nivel de log para USB y printer
    esp_log_level_set("USBH", ESP_LOG_DEBUG);
    esp_log_level_set("PRINTER", ESP_LOG_DEBUG);
    
    // Inicializar impresora con verificaciones
    ESP_LOGI(TAG, "Inicializando driver de impresora...");
    esp_err_t ret = printer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error inicializando impresora: %s", esp_err_to_name(ret));
    }

    // Esperar a que la impresora se detecte (máximo 5 segundos)
    int timeout = 50; // 50 * 100ms = 5 segundos
    while (!printer_is_ready() && timeout > 0) {
        ESP_LOGW(TAG, "⏳ Esperando impresora... (%d)", timeout);
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }

    if (!printer_is_ready()) {
        ESP_LOGE(TAG, "❌ Timeout esperando impresora");
    } else {
        ESP_LOGI(TAG, "✅ Impresora detectada y lista");

        printer_send_text(ESC_INIT);  // Inicializar impresora
        
        /* // Enviar mensaje de prueba corto
        const char *test_msg = "\n\nTEST IMPRESORA OK\n123456789\n\n\n";
        
        ESP_LOGI(TAG, "📝 Enviando mensaje de prueba...");
        ret = printer_send_text(test_msg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Error enviando mensaje: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "✅ Mensaje enviado correctamente");
        } */
    }
    
    // Pequeño delay antes de continuar
    vTaskDelay(pdMS_TO_TICKS(500));    
    ESP_LOGI(TAG, "Iniciando WiFi y servidor web...");
    wifi_manager_init_ap();
    start_webserver();
    
    ESP_LOGI(TAG, "Sistema listo - Modo aplicación activo");
}

void app_main(void) {
    // Inicialización básica
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    nvs_storage_init();

    // Configuración GPIO para botón
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Leer modo desde NVS
    char boot_mode[16] = {0};
    nvs_get_str_value(BOOT_MODE_KEY, boot_mode, sizeof(boot_mode), BOOT_MODE_CONFIG);

    // Arrancar según modo
    if (strcmp(boot_mode, BOOT_MODE_CONFIG) == 0) {
        start_config_mode();
    } else {
        start_normal_mode();
    }

    // Tarea del botón
    xTaskCreate(button_monitor_task, "button_monitor_task", 4096, NULL, 5, NULL);
}