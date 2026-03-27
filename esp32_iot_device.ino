#include "config.h"
#include "globals.h"
#include "device_config.h"
#include "device_reset.h"
#include "config_portal.h"
#include "wifi_manager.h"
#include "websocket_client.h"
#include "buttons.h"
#include "event_queue.h"
#include "heartbeat.h"
#include "led_status.h"

// ================================
// Estado global
// ================================

bool wifiConnected = false;
bool wsConnected = false;
bool cloudReachable = false;
bool otaInProgress = false;
bool wifiConnecting = false;
bool wsInitialized = false;
bool deviceRegistered = false;
bool awaitingBtnAck = false;

String deviceId;
String awaitingBtnMsgId = "";

unsigned long uptimeSeconds = 0;

// ================================
// LED inteligente
// ================================

void UpdateLed()
{
  if (IsConfigPortalRunning())
  {
    LedStatus_Set(LED_CONFIG_PORTAL);
    LedStatus_Tick();
    return;
  }

  if (wifiConnecting && !wifiConnected)
  {
    LedStatus_Set(LED_WIFI_CONNECTING);
    LedStatus_Tick();
    return;
  }

  if (!wifiConnected)
  {
    LedStatus_Set(LED_OFFLINE);
    LedStatus_Tick();
    return;
  }

  if (wifiConnected && !wsConnected)
  {
    LedStatus_Set(LED_DEGRADED);
    LedStatus_Tick();
    return;
  }

  LedStatus_Set(LED_WS_CONNECTED);
  LedStatus_Tick();
}

// ================================
// Setup
// ================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 IoT Device Boot");
  Serial.println("----------------------");

  // Generar deviceId único
  uint64_t chipid = ESP.getEfuseMac();
  char id[13];

  sprintf(id, "%04X%08X",
          (uint16_t)(chipid >> 32),
          (uint32_t)chipid);

  deviceId = String(id);

  Serial.print("DEVICE ID: ");
  Serial.println(deviceId);

  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());

  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());

  Serial.print("CPU Cores: ");
  Serial.println(ESP.getChipCores());

  Serial.print("CPU Frequency: ");
  Serial.println(getCpuFrequencyMhz());

  Serial.print("Flash Size: ");
  Serial.println(ESP.getFlashChipSize());

  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());

  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());

  // LED
  LedStatus_Init(LED_PIN);
  LedStatus_Set(LED_BOOT);

  // Inicializar config persistente
  DeviceConfig_Init();

  // ================================
  // Limpieza forzada (solo desarrollo)
  // ================================
#if FORCE_CLEAR_WIFI_CONFIG_ON_BOOT
  Serial.println("FORCE CLEAR WIFI CONFIG ON BOOT");
  DeviceConfig_ClearWifiConfig();
#endif

  // ================================
  // Reset físico (GPIO4)
  // ================================
  if (IsConfigResetRequested())
  {
    Serial.println("Clearing stored WiFi configuration...");
    DeviceConfig_ClearWifiConfig();

    delay(500);
    ESP.restart();
  }

  // ================================
  // Flujo principal
  // ================================
  if (DeviceConfig_HasWifiConfig())
  {
    Serial.println("[BOOT] WiFi config found -> normal mode");
    initWiFi();
  }
  else
  {
    Serial.println("[BOOT] No WiFi config -> starting portal");
    StartConfigPortal();
  }

  // Inicializar módulos
  initButtons();
  Heartbeat_Init();
}

// ================================
// Loop principal
// ================================

void loop()
{
  uptimeSeconds = millis() / 1000UL;

  // ================================
  // Portal activo
  // ================================
  if (IsConfigPortalRunning())
  {
    UpdateLed();
    LoopConfigPortal();
    return;
  }

  // ================================
  // WiFi
  // ================================
  loopWiFi();

  // ================================
  // WebSocket
  // ================================
  if (wifiConnected && !wsInitialized)
  {
    Serial.println("[WS] Initializing WebSocket...");
    initWebSocket();
    wsInitialized = true;
  }

  if (!wifiConnected)
  {
    wsInitialized = false;
  }

  loopWebSocket();

  // ================================
  // Botones
  // ================================
  loopButtons();

  // ================================
  // Heartbeat
  // ================================
HeartbeatData hb;
hb.deviceId = deviceId.c_str();
hb.wsConnected = wsConnected;
hb.eventQueueSize = eventQueueSize();

Serial.print("[HB] wsConnected(loop)=");
Serial.println(wsConnected ? "true" : "false");

Heartbeat_Tick(hb);

  // ================================
  // LED
  // ================================
  UpdateLed();
}