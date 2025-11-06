#include <stdio.h>
#include "esp_log.h"
//#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"

static const char *TAG = "OTA";

// Certificado opcional (puede ser NULL si usás HTTP o HTTPS sin verificación)
extern const char server_cert_pem[] asm("_binary_server_cert_pem_start");

esp_err_t ota_perform_update(const char *firmware_url)
{
    ESP_LOGI(TAG, "Iniciando OTA desde: %s", firmware_url);

    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .cert_pem = server_cert_pem,  // Poner NULL si usás HTTP
        .timeout_ms = 5000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA finalizada. Reiniciando...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Error en OTA: %s", esp_err_to_name(ret));
    }

    return ret;
}
