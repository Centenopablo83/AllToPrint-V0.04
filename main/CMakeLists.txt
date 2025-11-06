idf_component_register(
    SRCS "app_selector.c" "app_votacion.c" "wifi_manager.c" "main.c" "msg_manager.c" "nvs_storage.c" "printer_driver.c" "web_server.c" "ota_config_server.c" app_preguntas.c app_selector.c
    INCLUDE_DIRS "."
    REQUIRES log esp_http_client nvs_flash esp_http_server app_update esp_wifi esp_netif esp_timer esp_driver_gpio
)