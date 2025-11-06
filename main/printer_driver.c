#include "printer_driver.h"
#include "esp_log.h"


static const char *TAG = "PRINTER";


esp_err_t printer_init(void)
{
// FUTURO: inicializar USB Host y reclamar interfaz de tu Epson TM-U220D
ESP_LOGI(TAG, "Stub printer_init()");
return ESP_OK;
}


esp_err_t printer_send_text(const char *text)
{
// FUTURO: enviar por USB (ESC/POS). Por ahora sólo loguea.
ESP_LOGI(TAG, "Imprimir: %s", text? text : "<null>");
return ESP_OK;
}