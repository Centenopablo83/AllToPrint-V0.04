#include "app_interface.h"
#include "esp_log.h"

static const char *TAG = "APP_SELECTOR";

// Declarar funciones de las apps
const app_interface_t *get_app_preguntas(void);
const app_interface_t *get_app_votacion(void);

const app_interface_t *get_active_app(void) {
    // 🔄 CAMBIA ESTA LÍNEA PARA PROBAR DIFERENTES APPS
    // Para probar votación:
    ESP_LOGI(TAG, "App activa: Preguntas");
    return get_app_preguntas();
    
    // Para volver a preguntas:
    // ESP_LOGI(TAG, "App activa: Preguntas Anónimas");
    // return get_app_preguntas();
}