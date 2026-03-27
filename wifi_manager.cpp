#include <WiFi.h>
#include "globals.h"
#include "device_config.h"
#include "config.h"

static String currentSsid;
static String currentPassword;

static void beginWifiConnection() {
  if (currentSsid.length() == 0) {
    Serial.println("[WIFI] Invalid config: SSID empty");
    return;
  }

  Serial.print("[WIFI] Connecting to SSID: ");
  Serial.println(currentSsid);

  WiFi.begin(currentSsid.c_str(), currentPassword.c_str());
}

void initWiFi() {
  Serial.println("[WIFI] Init");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  WifiConfig cfg = DeviceConfig_GetWifiConfig();

  currentSsid = cfg.ssid;
  currentPassword = cfg.password;

  Serial.print("[CONFIG] Loaded SSID: ");
  Serial.println(currentSsid);

  beginWifiConnection();

  wifiConnected = false;
  wifiConnecting = true;
}

void loopWiFi() {
  static unsigned long lastAttempt = 0;
  static bool wasConnected = false;

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    wifiConnected = true;
    wifiConnecting = false;

    if (!wasConnected) {
      wasConnected = true;

      Serial.println("[WIFI] Connected");

      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());

      Serial.print("[WIFI] RSSI: ");
      Serial.println(WiFi.RSSI());
    }

    return;
  }

  wifiConnected = false;
  wifiConnecting = true;

  if (wasConnected) {
    wasConnected = false;
    Serial.println("[WIFI] Lost connection");
  }

  if (millis() - lastAttempt >= 5000UL) {
    Serial.println("[WIFI] Reconnecting...");

    WiFi.disconnect();
    beginWifiConnection();

    lastAttempt = millis();
  }
}