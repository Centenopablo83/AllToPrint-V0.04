#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WiFiMgr";

#define WIFI_AP_SSID      "AllToPrint-%02X%02X"
#define WIFI_AP_PASS      "12345678"
#define WIFI_MAX_CONN     4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          5

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;
static bool sta_connected = false;

static void wifi_event_handler_ap(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "✅ Punto de acceso iniciado");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGW(TAG, "⚠️ Punto de acceso detenido");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "📱 Cliente conectado: MAC=" MACSTR, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "📴 Cliente desconectado: MAC=" MACSTR, MAC2STR(event->mac));
    }
}

static void wifi_event_handler_sta(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "🔄 Conectando a WiFi...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "⚠️ Reintentando conexión (%d/%d)...", s_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "❌ Falló la conexión después de %d intentos", MAX_RETRY);
        }
        sta_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "✅ IP asignada: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        sta_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_init_ap(void)
{
    // Inicializa TCP/IP si no está inicializado
    static bool netif_initialized = false;
    if (!netif_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        netif_initialized = true;
    }

    // Crea interfaz de AP
    if (!ap_netif) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    // Inicializa WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registramos eventos AP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler_ap,
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

    // Modo AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "📡 WiFi AP inicializado:");
    ESP_LOGI(TAG, "  SSID: %s", wifi_config.ap.ssid);
    ESP_LOGI(TAG, "  PASS: %s", wifi_config.ap.password);
    ESP_LOGI(TAG, "  IP: 192.168.4.1");
}

esp_err_t wifi_manager_init_sta(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "❌ SSID inválido");
        return ESP_ERR_INVALID_ARG;
    }

    // Inicializa TCP/IP si no está inicializado
    static bool netif_initialized = false;
    if (!netif_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        netif_initialized = true;
    }

    // Crear grupo de eventos si no existe
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    // Crea interfaz de STA
    if (!sta_netif) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    // Inicializa WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar eventos STA
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler_sta,
        NULL,
        NULL
    ));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler_sta,
        NULL,
        NULL
    ));

    // Configuración STA
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password && strlen(password) > 0) {
        strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "🔄 Intentando conectar a: %s", ssid);

    // Esperar conexión o fallo
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000)  // Timeout de 15 segundos
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ Conectado exitosamente a %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ Falló la conexión a %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "❌ Timeout esperando conexión");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_get_sta_ip(char *ip_str)
{
    if (!ip_str || !sta_netif) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        snprintf(ip_str, 16, IPSTR, IP2STR(&ip_info.ip));
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

bool wifi_manager_is_sta_connected(void)
{
    return sta_connected;
}