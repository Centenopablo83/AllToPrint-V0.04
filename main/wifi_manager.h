#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el ESP32-S3 en modo Access Point
 *        con SSID dinámico "AllToPrint-XXYY"
 */
void wifi_manager_init_ap(void);

/**
 * @brief Inicializa el ESP32-S3 en modo Station
 *        para conectarse a una red WiFi existente
 * 
 * @param ssid SSID de la red WiFi
 * @param password Contraseña de la red WiFi
 * @return esp_err_t ESP_OK si se conectó exitosamente
 */
esp_err_t wifi_manager_init_sta(const char *ssid, const char *password);

/**
 * @brief Obtiene la IP asignada en modo STA
 * 
 * @param ip_str Buffer para almacenar la IP (mínimo 16 bytes)
 * @return esp_err_t ESP_OK si hay IP asignada
 */
esp_err_t wifi_manager_get_sta_ip(char *ip_str);

/**
 * @brief Verifica si está conectado en modo STA
 * 
 * @return true Si está conectado
 * @return false Si no está conectado
 */
bool wifi_manager_is_sta_connected(void);

#ifdef __cplusplus
}
#endif