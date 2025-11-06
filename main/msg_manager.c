#include "msg_manager.h"
#include "app_interface.h"
#include "esp_log.h"

static const char *TAG = "MSGS";

void msg_print(const char *msg) {
    if(!msg) {
        ESP_LOGW(TAG, "Mensaje nulo recibido, ignorado");
        return;
    }
    const app_interface_t *app = get_active_app();
    if(app && app->app_handle_message) {
        app->app_handle_message(msg);
    } else {
        ESP_LOGE(TAG, "No hay app activa para manejar mensajes");
    }
}