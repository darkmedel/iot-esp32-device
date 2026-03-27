#include "config.h"
#include "logger.h"
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

static void PrintBootSummary()
{
  LOG_INFO("");
  LOG_INFO("[BOOT] ESP32 IoT Device");
  LOG_INFOF("[BOOT] DeviceId: ", deviceId);
  LOG_INFOF("[BOOT] Firmware: ", FW_VERSION);

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
  LOG_DEBUGF("[BOOT] Chip Model: ", ESP.getChipModel());
  LOG_DEBUGF("[BOOT] Chip Revision: ", ESP.getChipRevision());
  LOG_DEBUGF("[BOOT] CPU Cores: ", ESP.getChipCores());
  LOG_DEBUGF("[BOOT] CPU Frequency MHz: ", getCpuFrequencyMhz());
  LOG_DEBUGF("[BOOT] Flash Size: ", ESP.getFlashChipSize());
  LOG_DEBUGF("[BOOT] Free Heap: ", ESP.getFreeHeap());
  LOG_DEBUGF("[BOOT] SDK Version: ", ESP.getSdkVersion());
#endif
}

// ================================
// Setup
// ================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  uint64_t chipid = ESP.getEfuseMac();
  char id[13];
  sprintf(id, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  deviceId = String(id);

  PrintBootSummary();

  LedStatus_Init(LED_PIN);
  LedStatus_Set(LED_BOOT);

  DeviceConfig_Init();

#if FORCE_CLEAR_WIFI_CONFIG_ON_BOOT
  LOG_WARN("[BOOT] FORCE_CLEAR_WIFI_CONFIG_ON_BOOT = true");
  DeviceConfig_ClearWifiConfig();
#endif

  if (IsConfigResetRequested())
  {
    LOG_WARN("[BOOT] Factory reset requested. Clearing stored WiFi configuration.");
    DeviceConfig_ClearWifiConfig();
    delay(500);
    ESP.restart();
  }

  if (DeviceConfig_HasWifiConfig())
  {
    LOG_INFO("[BOOT] WiFi config found -> normal mode");
    initWiFi();
  }
  else
  {
    LOG_WARN("[BOOT] No WiFi config -> starting portal");
    StartConfigPortal();
  }

  initButtons();
  Heartbeat_Init();
}

// ================================
// Loop principal
// ================================
void loop()
{
  uptimeSeconds = millis() / 1000UL;

  if (IsConfigPortalRunning())
  {
    UpdateLed();
    LoopConfigPortal();
    return;
  }

  loopWiFi();

  if (wifiConnected && !wsInitialized)
  {
    LOG_INFO("[WS] Initializing WebSocket...");
    initWebSocket();
    wsInitialized = true;
  }

  if (!wifiConnected)
  {
    wsInitialized = false;
  }

  loopWebSocket();
  loopButtons();

  HeartbeatData hb;
  hb.deviceId = deviceId.c_str();
  hb.wsConnected = wsConnected;
  hb.eventQueueSize = eventQueueSize();

  Heartbeat_Tick(hb);

  UpdateLed();
}