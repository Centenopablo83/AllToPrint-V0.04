#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el ESP32-S3 en modo Access Point
 *        con SSID dinámico "AllToPrint-XXYY"
 */
void wifi_manager_init_ap(void);

#ifdef __cplusplus
}
#endif