#include "printer_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include <string.h>

static const char *TAG = "PRINTER";

// 🔥 CONFIGURACIÓN AJUSTADA SEGÚN EL CÓDIGO QUE FUNCIONA
#define HOST_LIB_TASK_PRIORITY    2     // Mismo que el código que funciona
#define CLASS_TASK_PRIORITY       3     // Mismo que el código que funciona
#define PRINTER_INTERFACE_CLASS   0x07
#define PRINTER_VENDOR_CLASS      0xFF
#define PRINTER_INTERFACE_NUMBER  0
#define PRINTER_ENDPOINT_OUT      0x01

#define PRINT_QUEUE_SIZE          10
#define PRINT_BUFFER_SIZE         512
#define CLIENT_NUM_EVENT_MSG      5

// Estructura de trabajo de impresión
typedef struct {
    uint8_t data[PRINT_BUFFER_SIZE];
    size_t length;
} print_job_t;

// Estructura del driver
typedef struct {
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t dev_hdl;
    uint8_t dev_addr;
    bool printer_ready;
    QueueHandle_t print_queue;
    SemaphoreHandle_t mutex;
    TaskHandle_t usb_host_task_hdl;
    TaskHandle_t client_task_hdl;
    TaskHandle_t print_task_hdl;
    bool initialized;
    bool stop_usb_host;
} printer_driver_t;

static printer_driver_t s_printer = {0};

// ============================================
// PROTOTIPOS INTERNOS
// ============================================
static void usb_host_lib_task(void *arg);
static void client_task(void *arg);
static void printer_process_task(void *arg);
static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg);
static void transfer_callback(usb_transfer_t *transfer);
static esp_err_t open_printer_device(uint8_t dev_addr);
static esp_err_t send_to_usb_printer(const uint8_t *data, size_t length);

// ============================================
// CALLBACK DE TRANSFERENCIA USB
// ============================================
static void transfer_callback(usb_transfer_t *transfer)
{
    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_LOGE(TAG, "Transfer failed, status: %d", transfer->status);
    }
    usb_host_transfer_free(transfer);
}

// ============================================
// ENVÍO USB DIRECTO
// ============================================
static esp_err_t send_to_usb_printer(const uint8_t *data, size_t length)
{
    if (!s_printer.printer_ready || !s_printer.dev_hdl) {
        ESP_LOGE(TAG, "Impresora no lista");
        return ESP_ERR_NOT_FOUND;
    }
    
    usb_transfer_t *transfer = NULL;
    esp_err_t ret = usb_host_transfer_alloc(length, 0, &transfer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error allocating transfer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    memcpy(transfer->data_buffer, data, length);
    transfer->device_handle = s_printer.dev_hdl;
    transfer->num_bytes = length;
    transfer->callback = transfer_callback;
    transfer->context = NULL;
    transfer->bEndpointAddress = PRINTER_ENDPOINT_OUT;
    transfer->timeout_ms = 5000;
    
    ret = usb_host_transfer_submit(transfer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error submitting transfer: %s", esp_err_to_name(ret));
        usb_host_transfer_free(transfer);
        return ret;
    }
    
    return ESP_OK;
}

// ============================================
// CALLBACK DE EVENTOS DEL CLIENTE USB
// ============================================
static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    ESP_LOGI(TAG, "📢 Evento USB: %d", event_msg->event);
    
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "🔌 Nueva impresora detectada en addr %d", event_msg->new_dev.address);
            
            xSemaphoreTake(s_printer.mutex, portMAX_DELAY);
            s_printer.dev_addr = event_msg->new_dev.address;
            xSemaphoreGive(s_printer.mutex);
            
            // Abrir y reclamar en esta misma tarea
            open_printer_device(event_msg->new_dev.address);
            break;
            
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(TAG, "❌ Impresora desconectada");
            xSemaphoreTake(s_printer.mutex, portMAX_DELAY);
            if (s_printer.dev_hdl == event_msg->dev_gone.dev_hdl) {
                s_printer.printer_ready = false;
                s_printer.dev_hdl = NULL;
                s_printer.dev_addr = 0;
            }
            xSemaphoreGive(s_printer.mutex);
            break;
            
        default:
            ESP_LOGW(TAG, "Evento USB no manejado: %d", event_msg->event);
            break;
    }
}

// ============================================
// ABRIR Y RECLAMAR IMPRESORA
// ============================================
static esp_err_t open_printer_device(uint8_t dev_addr)
{
    esp_err_t ret;
    usb_device_handle_t dev_hdl = NULL;
    
    ESP_LOGI(TAG, "📂 Abriendo dispositivo en addr %d...", dev_addr);
    
    // Abrir dispositivo
    ret = usb_host_device_open(s_printer.client_hdl, dev_addr, &dev_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error abriendo dispositivo: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Dispositivo abierto, obteniendo descriptores...");
    
    // Obtener descriptor del dispositivo
    const usb_device_desc_t *dev_desc;
    ret = usb_host_get_device_descriptor(dev_hdl, &dev_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error obteniendo device descriptor");
        usb_host_device_close(s_printer.client_hdl, dev_hdl);
        return ret;
    }
    
    ESP_LOGI(TAG, "📋 Device Class: 0x%02x, VID: 0x%04x, PID: 0x%04x", 
             dev_desc->bDeviceClass, dev_desc->idVendor, dev_desc->idProduct);
    
    // Obtener descriptor de configuración
    const usb_config_desc_t *config_desc;
    ret = usb_host_get_active_config_descriptor(dev_hdl, &config_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error obteniendo config descriptor");
        usb_host_device_close(s_printer.client_hdl, dev_hdl);
        return ret;
    }
    
    ESP_LOGI(TAG, "📋 Num Interfaces: %d, Total Length: %d", 
             config_desc->bNumInterfaces, config_desc->wTotalLength);
    
    // Buscar interfaz de impresora
    bool found_printer = false;
    uint8_t interface_number = 0;
    
    const uint8_t *p = (const uint8_t *)(config_desc + 1);
    const uint8_t *end = ((const uint8_t *)config_desc) + config_desc->wTotalLength;
    
    while (p < end) {
        if (p[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
            
            ESP_LOGI(TAG, "🔍 Interface %d: Class 0x%02x, SubClass 0x%02x", 
                     intf->bInterfaceNumber, intf->bInterfaceClass, intf->bInterfaceSubClass);
            
            // Buscar clase impresora (0x07) o vendor-specific (0xFF)
            if (intf->bInterfaceClass == PRINTER_INTERFACE_CLASS || 
                intf->bInterfaceClass == PRINTER_VENDOR_CLASS) {
                found_printer = true;
                interface_number = intf->bInterfaceNumber;
                ESP_LOGI(TAG, "✅ Interfaz impresora encontrada: %d (clase 0x%02x)", 
                         interface_number, intf->bInterfaceClass);
                break;
            }
        }
        p += p[0]; // Siguiente descriptor
    }
    
    if (!found_printer) {
        ESP_LOGW(TAG, "⚠️ Dispositivo no es impresora compatible");
        usb_host_device_close(s_printer.client_hdl, dev_hdl);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "🔧 Reclamando interfaz %d...", interface_number);
    
    // Reclamar interfaz
    ret = usb_host_interface_claim(s_printer.client_hdl, dev_hdl, interface_number, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error reclamando interfaz: %s", esp_err_to_name(ret));
        usb_host_device_close(s_printer.client_hdl, dev_hdl);
        return ret;
    }
    
    // Guardar handle
    xSemaphoreTake(s_printer.mutex, portMAX_DELAY);
    s_printer.dev_hdl = dev_hdl;
    s_printer.dev_addr = dev_addr;
    s_printer.printer_ready = true;
    xSemaphoreGive(s_printer.mutex);
    
    ESP_LOGI(TAG, "🎉 ✅ Impresora reclamada y lista en addr %d", dev_addr);
    
    // Enviar comando de inicialización
    vTaskDelay(pdMS_TO_TICKS(200));
    const uint8_t init_cmd[] = {0x1B, 0x40}; // ESC @
    send_to_usb_printer(init_cmd, sizeof(init_cmd));
    
    ESP_LOGI(TAG, "📤 Comando de inicialización enviado");
    
    return ESP_OK;
}

// ============================================
// TAREA USB HOST LIBRARY (igual al código que funciona)
// ============================================
static void usb_host_lib_task(void *arg)
{
    ESP_LOGI(TAG, "🚀 Iniciando USB Host Library");
    
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error instalando USB Host: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "✅ USB Host instalado");
    
    // Notificar que USB Host está listo
    xTaskNotifyGive((TaskHandle_t)arg);
    
    // Loop de eventos USB
    bool has_clients = true;
    bool has_devices = false;
    
    while (!s_printer.stop_usb_host && has_clients) {
        uint32_t event_flags;
        ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Error en handle_events: %s", esp_err_to_name(ret));
            break;
        }
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "📢 FLAGS_NO_CLIENTS");
            if (usb_host_device_free_all() == ESP_OK) {
                ESP_LOGI(TAG, "✅ Todos los dispositivos liberados");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "⏳ Esperando FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        
        if (has_devices && (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)) {
            ESP_LOGI(TAG, "📢 FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    
    ESP_LOGI(TAG, "🛑 Deteniendo USB Host Library");
    usb_host_uninstall();
    vTaskDelete(NULL);
}

// ============================================
// TAREA CLIENTE USB
// ============================================
static void client_task(void *arg)
{
    ESP_LOGI(TAG, "🔧 Iniciando tarea cliente USB");
    
    // Crear mutex si no existe
    if (!s_printer.mutex) {
        s_printer.mutex = xSemaphoreCreateMutex();
        if (!s_printer.mutex) {
            ESP_LOGE(TAG, "❌ Error creando mutex");
            vTaskDelete(NULL);
            return;
        }
    }
    
    // Registrar cliente USB
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = CLIENT_NUM_EVENT_MSG,
        .async = {
            .client_event_callback = client_event_callback,
            .callback_arg = &s_printer,
        },
    };
    
    esp_err_t ret = usb_host_client_register(&client_config, &s_printer.client_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error registrando cliente USB: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "✅ Cliente USB registrado");
    
    // Loop de eventos del cliente
    while (!s_printer.stop_usb_host) {
        usb_host_client_handle_events(s_printer.client_hdl, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "🛑 Desregistrando cliente");
    usb_host_client_deregister(s_printer.client_hdl);
    s_printer.client_hdl = NULL;
    vTaskDelete(NULL);
}

// ============================================
// TAREA PROCESADORA DE COLA DE IMPRESIÓN
// ============================================
static void printer_process_task(void *arg)
{
    print_job_t job;
    
    ESP_LOGI(TAG, "🖨️ Tarea de impresión iniciada");
    
    while (1) {
        // Esperar trabajos en la cola
        if (xQueueReceive(s_printer.print_queue, &job, portMAX_DELAY)) {
            
            // Esperar que la impresora esté lista
            while (!s_printer.printer_ready) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            
            // Enviar a USB
            esp_err_t ret = send_to_usb_printer(job.data, job.length);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Enviados %d bytes a impresora", job.length);
            } else {
                ESP_LOGE(TAG, "❌ Error enviando a impresora");
            }
            
            // Pequeño delay entre trabajos
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

// ============================================
// API PÚBLICA
// ============================================

esp_err_t printer_init(void)
{
    if (s_printer.initialized) {
        ESP_LOGW(TAG, "⚠️ Driver ya inicializado");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🚀 Inicializando driver de impresora USB...");
    
    // Crear mutex
    s_printer.mutex = xSemaphoreCreateMutex();
    if (!s_printer.mutex) {
        ESP_LOGE(TAG, "❌ Error creando mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Crear cola de impresión
    s_printer.print_queue = xQueueCreate(PRINT_QUEUE_SIZE, sizeof(print_job_t));
    if (!s_printer.print_queue) {
        ESP_LOGE(TAG, "❌ Error creando cola");
        vSemaphoreDelete(s_printer.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    s_printer.initialized = true;
    s_printer.stop_usb_host = false;
    
    // 1. Crear tarea USB Host (prioridad 2)
    BaseType_t ret = xTaskCreatePinnedToCore(
        usb_host_lib_task,
        "usb_host",
        4096,  // Mismo stack que el código que funciona
        xTaskGetCurrentTaskHandle(),
        HOST_LIB_TASK_PRIORITY,
        &s_printer.usb_host_task_hdl,
        0
    );
    
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "❌ Error creando tarea USB Host");
        printer_deinit();
        return ESP_FAIL;
    }
    
    // Esperar que USB Host esté listo
    ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "✅ USB Host listo");
    
    // 2. Crear tarea cliente (prioridad 3)
    ret = xTaskCreatePinnedToCore(
        client_task,
        "usb_client",
        5 * 1024,  // Mismo stack que el código que funciona
        NULL,
        CLASS_TASK_PRIORITY,
        &s_printer.client_task_hdl,
        0
    );
    
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "❌ Error creando tarea cliente");
        printer_deinit();
        return ESP_FAIL;
    }
    
    // Delay para que el cliente se registre
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 3. Crear tarea procesadora de impresión (prioridad 4)
    ret = xTaskCreatePinnedToCore(
        printer_process_task,
        "print_queue",
        3 * 1024,
        NULL,
        4,  // Prioridad menor que cliente
        &s_printer.print_task_hdl,
        0
    );
    
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "❌ Error creando tarea de impresión");
        printer_deinit();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Driver inicializado, esperando impresora USB...");
    return ESP_OK;
}

esp_err_t printer_send_raw(const uint8_t *data, size_t length)
{
    if (!s_printer.initialized) {
        ESP_LOGE(TAG, "❌ Driver no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || length == 0) {
        ESP_LOGE(TAG, "❌ Datos inválidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (length > PRINT_BUFFER_SIZE) {
        ESP_LOGE(TAG, "❌ Datos muy largos (%d bytes, máx %d)", length, PRINT_BUFFER_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Crear trabajo de impresión
    print_job_t job;
    memcpy(job.data, data, length);
    job.length = length;
    
    // Encolar (con timeout de 1 segundo)
    if (xQueueSend(s_printer.print_queue, &job, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Cola de impresión llena");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t printer_send_text(const char *text)
{
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t len = strlen(text);
    ESP_LOGI(TAG, "📝 Encolando %d bytes para impresión", len);
    return printer_send_raw((const uint8_t *)text, len);
}

bool printer_is_ready(void)
{
    bool ready = false;
    if (s_printer.mutex) {
        xSemaphoreTake(s_printer.mutex, portMAX_DELAY);
        ready = s_printer.printer_ready;
        xSemaphoreGive(s_printer.mutex);
    }
    return ready;
}

void printer_deinit(void)
{
    if (!s_printer.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "🛑 Deteniendo driver de impresora...");
    
    s_printer.initialized = false;
    s_printer.stop_usb_host = true;
    
    // Liberar impresora si está conectada
    if (s_printer.dev_hdl) {
        usb_host_interface_release(s_printer.client_hdl, s_printer.dev_hdl, PRINTER_INTERFACE_NUMBER);
        usb_host_device_close(s_printer.client_hdl, s_printer.dev_hdl);
        s_printer.dev_hdl = NULL;
    }
    
    // Eliminar tareas
    if (s_printer.print_task_hdl) {
        vTaskDelete(s_printer.print_task_hdl);
        s_printer.print_task_hdl = NULL;
    }
    
    if (s_printer.client_task_hdl) {
        vTaskDelete(s_printer.client_task_hdl);
        s_printer.client_task_hdl = NULL;
    }
    
    // Eliminar cola y mutex
    if (s_printer.print_queue) {
        vQueueDelete(s_printer.print_queue);
        s_printer.print_queue = NULL;
    }
    
    if (s_printer.mutex) {
        vSemaphoreDelete(s_printer.mutex);
        s_printer.mutex = NULL;
    }
    
    s_printer.printer_ready = false;
    
    ESP_LOGI(TAG, "✅ Driver detenido");
}