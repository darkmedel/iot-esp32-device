#include "device_config.h"
#include "config.h"

#include <Preferences.h>

static Preferences prefs;
static const char* PREF_NAMESPACE = "iotcfg";
static const char* KEY_WIFI_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";

void DeviceConfig_Init() {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.end();
}

bool DeviceConfig_HasWifiConfig() {
  prefs.begin(PREF_NAMESPACE, true);

  String ssid = prefs.getString(KEY_WIFI_SSID, "");
  bool hasConfig = ssid.length() > 0;

  prefs.end();
  return hasConfig;
}

WifiConfig DeviceConfig_GetWifiConfig() {
  WifiConfig cfg;

  prefs.begin(PREF_NAMESPACE, true);

  cfg.ssid = prefs.getString(KEY_WIFI_SSID, "");
  cfg.password = prefs.getString(KEY_WIFI_PASS, "");

  prefs.end();

  // Fallback opcional a defaults
  if (cfg.ssid.length() == 0) {
    cfg.ssid = DEFAULT_WIFI_SSID;
    cfg.password = DEFAULT_WIFI_PASSWORD;
  }

  return cfg;
}

bool DeviceConfig_SaveWifiConfig(const String& ssid, const String& password) {
  if (ssid.length() == 0) {
    return false;
  }

  prefs.begin(PREF_NAMESPACE, false);

  bool ok1 = prefs.putString(KEY_WIFI_SSID, ssid) > 0;
  bool ok2 = prefs.putString(KEY_WIFI_PASS, password) >= 0;

  prefs.end();

  return ok1 && ok2;
}

void DeviceConfig_ClearWifiConfig() {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.remove(KEY_WIFI_SSID);
  prefs.remove(KEY_WIFI_PASS);
  prefs.end();
}