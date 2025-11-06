idf_component_register(
    SRCS "ota_update.c"
    INCLUDE_DIRS "."
    REQUIRES log esp_http_client nvs_flash esp_http_server ota_update esp_https_ota esp_wifi esp_netif esp_driver_gpio
)
