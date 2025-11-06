#include "nvs_storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>


static const char *TAG = "NVS";
static const char *NS = "app"; // namespace


void nvs_storage_init(void) {
// Ya se llamó nvs_flash_init() en app_main, acá nada por ahora.
}


esp_err_t nvs_set_str_value(const char *key, const char *value)
{
nvs_handle_t h; esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
if (err != ESP_OK) return err;
err = nvs_set_str(h, key, value);
if (err == ESP_OK) err = nvs_commit(h);
nvs_close(h);
ESP_LOGI(TAG, "[%s] = '%s' (%s)", key, value? value:"", esp_err_to_name(err));
return err;
}


esp_err_t nvs_get_str_value(const char *key, char *out, size_t outlen, const char *def)
{
if (!out || outlen==0) return ESP_ERR_INVALID_ARG;
nvs_handle_t h; esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
if (err != ESP_OK) {
if (def) { strncpy(out, def, outlen); out[outlen-1]='\0'; return ESP_OK; }
return err;
}
size_t needed = outlen;
err = nvs_get_str(h, key, out, &needed);
nvs_close(h);
if (err == ESP_OK) return ESP_OK;
if (def) { strncpy(out, def, outlen); out[outlen-1]='\0'; return ESP_OK; }
return err;
}