#ifndef CONFIG_H
#define CONFIG_H

// ================================
// Firmware
// ================================
#define FW_VERSION "1.0"

// ================================
// Logging
// ================================
// Producción recomendada: LOG_LEVEL_INFO
// Laboratorio/diagnóstico: LOG_LEVEL_DEBUG
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO

// ================================
// WiFi por defecto / provisión
// ================================
// Si no quieres fallback, déjalos vacíos.
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Forzar limpieza de configuración WiFi al arrancar.
// Útil para dejar el equipo "virgen" al cargar firmware.
#define FORCE_CLEAR_WIFI_CONFIG_ON_BOOT false

// Si hay configuración guardada pero no conecta dentro de este tiempo,
// se puede levantar portal de rescate (opcional).
#define WIFI_CONNECT_TIMEOUT 60000UL

// ================================
// Portal de configuración
// ================================
#define CONFIG_PORTAL_AP_PASSWORD ""
#define CONFIG_PORTAL_PORT 80

// ================================
// Config Reset (botón de servicio)
// ================================
#define CONFIG_RESET_BUTTON_PIN 4
#define CONFIG_RESET_HOLD_MS 10000UL

// ================================
// WebSocket / Gateway IoT
// ================================
#define WS_HOST "192.168.1.112"
#define WS_PORT 5000
#define WS_PATH "/ws"

#define WS_PING_INTERVAL 15000UL
#define WS_RECONNECT_INTERVAL 10000UL
#define WS_PONG_TIMEOUT 10000UL
#define WS_INACTIVITY_TIMEOUT 30000UL
#define WS_MAX_RECONNECT_FAILURES 3

// ================================
// Telemetría / Estado
// ================================
#define STATUS_INTERVAL 15000UL

// ================================
// Heartbeat / Watchtower
// ================================
#define HEARTBEAT_INTERVAL 60000UL
#define HEARTBEAT_URL "http://192.168.1.112:5009/heartbeat"
#define HEARTBEAT_CONNECT_TIMEOUT 1500
#define HEARTBEAT_REQUEST_TIMEOUT 2500

// Si falla el heartbeat muchas veces, no inundar el puerto serie.
#define HEARTBEAT_FAIL_LOG_INTERVAL 60000UL

// ================================
// Botones / Eventos
// ================================
#define BUTTON_COUNT 6
#define BTN_SCAN_INTERVAL 20
#define BTN_DEBOUNCE 50
#define MAX_EVENT_QUEUE 100

// ================================
// Hardware
// ================================
#define LED_PIN 2

#endif