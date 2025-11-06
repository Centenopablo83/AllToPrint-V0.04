#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>
#include "esp_mac.h"  // Necesario para MACSTR y MAC2STR en ESP-IDF v5.x

static const char *TAG = "WiFiAP";

#define WIFI_AP_SSID      "AllToPrint-%02X%02X"
#define WIFI_AP_PASS      "12345678"  // Min 8 chars para WPA2
#define WIFI_MAX_CONN     4

static esp_netif_t *ap_netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Punto de acceso iniciado");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGW(TAG, "Punto de acceso detenido");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Cliente conectado: MAC=" MACSTR, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Cliente desconectado: MAC=" MACSTR, MAC2STR(event->mac));
    }
}

void wifi_manager_init_ap(void)
{
    // Inicializa TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Crea interfaz de AP
    ap_netif = esp_netif_create_default_wifi_ap();

    // Inicializa WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registramos eventos
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    // Configuración de la red AP
    wifi_config_t wifi_config = {0};
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid),
             WIFI_AP_SSID, mac[4], mac[5]);
    strcpy((char *)wifi_config.ap.password, WIFI_AP_PASS);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = WIFI_MAX_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    // Si querés AP abierto, descomentar esto:
    // wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    // wifi_config.ap.password[0] = '\0';

    // Modo AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP inicializado:");
    ESP_LOGI(TAG, "  SSID: %s", wifi_config.ap.ssid);
    ESP_LOGI(TAG, "  PASS: %s", wifi_config.ap.password);
    ESP_LOGI(TAG, "  Canal: %d", wifi_config.ap.channel);
}