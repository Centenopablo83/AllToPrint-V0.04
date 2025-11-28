# 🖨️ AllToPrint - Sistema de Impresión USB WiFi

Sistema embebido basado en ESP32-S3 que convierte cualquier impresora térmica USB en un punto de acceso WiFi para impresión remota.

## 📋 Características

### ✨ Principales
- 🔌 **Driver USB Host** para impresoras térmicas ESC/POS
- 📡 **Dual WiFi Mode**: Access Point y Station
- 🌐 **Portal de Configuración Web** para escaneo y conexión WiFi
- 🔄 **OTA Updates** via web interface
- 📱 **Apps Intercambiables**: Sistema modular de aplicaciones
- 💾 **Persistencia NVS** para credenciales y configuración
- 🔘 **Dual Boot Mode**: Normal y Configuración (via botón BOOT)

### 🎯 Apps Incluidas
1. **Preguntas Anónimas**: Buzón digital que imprime mensajes
2. **Votación**: Sistema de votación con impresión de comprobantes

## 🛠️ Hardware Requerido

- **ESP32-S3** (con soporte USB Host)
- **Impresora Térmica USB** compatible con ESC/POS
- Cable USB con datos (no solo carga)
- Fuente de alimentación adecuada (5V, min 2A)

## 📦 Estructura del Proyecto

```
AllToPrint/
├── main/
│   ├── main.c                  # Punto de entrada principal
│   ├── app_interface.h         # Interface para apps intercambiables
│   ├── app_selector.c          # Selector de app activa
│   ├── app_preguntas.c         # App de preguntas anónimas
│   ├── app_votacion.c          # App de votación
│   ├── printer_driver.c/h      # Driver USB para impresora
│   ├── wifi_manager.c/h        # Gestión WiFi (AP + Station)
│   ├── wifi_config_server.c/h  # Portal configuración WiFi
│   ├── web_server.c/h          # Servidor web principal
│   ├── ota_config_server.c/h   # Servidor OTA
│   ├── nvs_storage.c/h         # Gestión de NVS
│   └── msg_manager.c/h         # Gestión de mensajes
├── components/
│   └── ota_update/             # Componente OTA (opcional)
├── partitions.csv              # Tabla de particiones
├── CMakeLists.txt
└── README.md
```

## 🚀 Compilación y Flash

### Prerrequisitos
```bash
# Instalar ESP-IDF v5.x
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

### Compilar
```bash
# Configurar target
idf.py set-target esp32s3

# Configurar proyecto (opcional)
idf.py menuconfig

# Compilar
idf.py build

# Flash y monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

### Configuración Importante

En `idf.py menuconfig`:
- Component config → ESP32-specific → **Support for external USB PHY chip → Enable**
- Component config → FreeRTOS → **Kernel tick rate → 1000 Hz**

## 🎮 Modos de Operación

### 🔧 Modo CONFIGURACIÓN (AP_config)

**Activación**: 
- Primera vez después del flash
- Mantener botón BOOT presionado 5 segundos

**Funcionalidades**:
1. **OTA Update** (`http://192.168.4.1/`)
   - Subir firmware `.bin` para cambiar aplicación
   - Sistema se reinicia automáticamente

2. **WiFi Config** (`http://192.168.4.1/wifi`)
   - Escaneo de redes disponibles
   - Conexión a red WiFi
   - Guarda credenciales en NVS

**Red WiFi AP**:
- SSID: `AllToPrint-XXYY` (XX YY son últimos bytes de MAC)
- Password: `12345678`
- IP: `192.168.4.1`

### ▶️ Modo NORMAL

**Activación**: 
- Automático después de configurar
- Mantener botón BOOT 5 segundos para alternar desde config

**Comportamiento**:
1. Intenta conectar a WiFi guardado en NVS
2. Si falla, inicia modo AP como fallback
3. Inicializa impresora USB
4. Inicia aplicación activa
5. Servidor web disponible para interacción

**Acceso**:
- Si conectado a WiFi: `http://<IP_ASIGNADA>`
- Si en modo AP: `http://192.168.4.1`

## 📱 Uso de las Apps

### App Preguntas Anónimas

**Interfaz Web**: `http://<IP>/`

**Flujo**:
1. Usuario escribe mensaje (máx 200 caracteres)
2. Sistema verifica que impresora esté lista
3. Al enviar, imprime ticket formateado:
   ```
   ================================
        PREGUNTA ANONIMA
   ================================   
    
   ¿Cuál es el sentido de la vida?
   
   ================================
        AllToPrint - Preguntas
   ================================
   ```

**Endpoints**:
- `GET /` - Formulario web
- `POST /msg` - Enviar mensaje
- `GET /printer_status` - Estado impresora (JSON)

### App Votación

**Interfaz Web**: `http://<IP>/`

**Flujo**:
1. Usuario selecciona opción (1, 2 o 3)
2. Sistema imprime comprobante de voto
3. Confirma en pantalla

**Endpoints**:
- `GET /` - Formulario votación
- `POST /vote` - Registrar voto

## 🔄 Cambiar de Aplicación

### Método 1: Via OTA (Recomendado)

1. Entrar en modo configuración (botón BOOT 5s)
2. Ir a `http://192.168.4.1/`
3. Compilar nueva app:
   ```bash
   # En app_selector.c cambiar:
   return get_app_preguntas();  // o get_app_votacion()
   
   idf.py build
   ```
4. Subir archivo `build/AllToPrint.bin`
5. Sistema se reinicia con nueva app

### Método 2: Via Flash

```bash
# Editar app_selector.c
# Recompilar y flashear
idf.py -p /dev/ttyUSB0 flash
```

## 🆕 Crear Nueva Aplicación

### 1. Crear archivo de app

```c
// main/app_mi_nueva_app.c
#include "app_interface.h"
#include "printer_driver.h"
#include "esp_log.h"

static const char *TAG = "MI_APP";

// HTML de la interfaz
const char *html_mi_app = 
"<!DOCTYPE html>"
"<html>..."
"</html>";

// Inicialización
static void app_init(void) {
    ESP_LOGI(TAG, "Mi App inicializada");
}

// Procesar mensaje
static void app_handle_message(const char *msg) {
    printer_send_text(msg);
}

// Handler HTTP GET /
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_sendstr(req, html_mi_app);
    return ESP_OK;
}

// Registrar endpoints
static void app_register_http_handlers(httpd_handle_t server) {
    httpd_uri_t uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    httpd_register_uri_handler(server, &uri);
}

// Exponer app
const app_interface_t *get_app_mi_nueva_app(void) {
    static const app_interface_t app = {
        .app_init = app_init,
        .app_handle_message = app_handle_message,
        .app_register_http_handlers = app_register_http_handlers,
        .app_get_html = NULL
    };
    return &app;
}
```

### 2. Registrar en selector

```c
// main/app_selector.c
const app_interface_t *get_app_mi_nueva_app(void);  // Declarar

const app_interface_t *get_active_app(void) {
    return get_app_mi_nueva_app();  // Activar
}
```

### 3. Actualizar CMakeLists.txt

```cmake
idf_component_register(
    SRCS 
        ...
        "app_mi_nueva_app.c"  # Agregar
    ...
)
```

## 🖨️ Comandos ESC/POS Disponibles

```c
// Alineación
ESC_ALIGN_LEFT
ESC_ALIGN_CENTER
ESC_ALIGN_RIGHT

// Estilos
ESC_BOLD_ON / ESC_BOLD_OFF
ESC_UNDERLINE_ON / ESC_UNDERLINE_OFF
ESC_INVERSE_ON / ESC_INVERSE_OFF

// Tamaños
ESC_NORMAL_SIZE
ESC_DOUBLE_HEIGHT
ESC_DOUBLE_WIDTH
ESC_DOUBLE_SIZE_ON / ESC_DOUBLE_SIZE_OFF

// Control papel
ESC_LINEFEED
ESC_FEED_1 / ESC_FEED_2 / ESC_FEED_3
ESC_CUT_PARTIAL / ESC_CUT_FULL

// Inicialización
ESC_INIT
```

### Ejemplo de uso:

```c
char buffer[256];
snprintf(buffer, sizeof(buffer),
    "%s%s"        // Centro + Grande
    "TITULO\n"
    "%s%s"        // Izquierda + Normal
    "Texto normal\n"
    "%s%s",       // Feed + Corte
    ESC_ALIGN_CENTER, ESC_DOUBLE_SIZE_ON,
    ESC_ALIGN_LEFT, ESC_NORMAL_SIZE,
    ESC_FEED_2, ESC_CUT_PARTIAL
);

printer_send_raw((uint8_t*)buffer, strlen(buffer));
```

## 🔧 API de WiFi Manager

```c
// Escanear redes
wifi_ap_record_simple_t networks[20];
uint16_t count;
wifi_manager_scan(networks, 20, &count);

// Conectar a red
wifi_manager_connect_sta("Mi_WiFi", "password", NULL);

// Verificar conexión
if (wifi_manager_is_connected()) {
    char ip[16];
    wifi_manager_get_ip(ip, sizeof(ip));
}

// Guardar credenciales
wifi_manager_save_credentials("SSID", "pass");

// Cargar credenciales
char ssid[33], pass[64];
wifi_manager_load_credentials(ssid, pass);

// Borrar credenciales
wifi_manager_clear_credentials();
```

## 🐛 Troubleshooting

### Impresora no detectada
- Verificar cable USB (debe soportar datos, no solo carga)
- Verificar fuente de alimentación (min 2A)
- Revisar logs: `esp_log_level_set("USBH", ESP_LOG_DEBUG);`
- Probar con otra impresora compatible ESC/POS

### No aparece red WiFi
- Verificar que está en modo configuración
- Buscar red `AllToPrint-XXYY`
- Reiniciar ESP32
- Revisar que WiFi AP esté habilitado en menuconfig

### Error al flashear
```bash
# Borrar flash completo
idf.py erase-flash

# Flash con particiones
idf.py -p /dev/ttyUSB0 flash
```

### Memoria insuficiente
- Reducir `config.stack_size` en httpd_config
- Ajustar `CONFIG_FREERTOS_HZ` a 1000
- Optimizar buffers en apps

## 📊 Configuración de Particiones

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000,
phy_init,  data, phy,     0xf000,   0x1000,
factory,   app,  factory, 0x10000,  2M,      # App inicial
ota_0,     app,  ota_0,   0x210000, 2M,      # OTA slot 0
ota_1,     app,  ota_1,   0x410000, 2M,      # OTA slot 1
otadata,   data, ota,     0x610000, 0x2000,  # Metadatos OTA
storage,   data, spiffs,  0x612000, 0xFE000  # Storage futuro
```

## 📝 Logs de Debugging

```bash
# Ver todos los logs
idf.py monitor

# Filtrar por componente
idf.py monitor | grep "PRINTER"
idf.py monitor | grep "WiFiMgr"

# Nivel de log por componente (en código)
esp_log_level_set("PRINTER", ESP_LOG_DEBUG);
esp_log_level_set("USBH", ESP_LOG_DEBUG);
esp_log_level_set("WiFiMgr", ESP_LOG_INFO);
```

## 🤝 Contribuir

1. Fork el proyecto
2. Crear branch de feature (`git checkout -b feature/nueva-app`)
3. Commit cambios (`git commit -am 'Agregar nueva app'`)
4. Push al branch (`git push origin feature/nueva-app`)
5. Crear Pull Request

## 📄 Licencia

[Tu licencia aquí]

## 👨‍💻 Autor

Pablo Centeno
centenopablo@yahoo.com.ar

---

## 🎉 Próximas Features

- [ ] Modo Bridge (AP + STA simultáneo)
- [ ] MQTT Support para IoT
- [ ] Web interface para gestión de apps
- [ ] Sistema de logs remotos
- [ ] Soporte para múltiples impresoras
- [ ] Templates de impresión configurables
- [ ] API REST completa

---

**Hecho con ❤️ para la comunidad maker**