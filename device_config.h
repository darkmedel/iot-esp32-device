#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

struct WifiConfig {
  String ssid;
  String password;
};

void DeviceConfig_Init();

bool DeviceConfig_HasWifiConfig();
WifiConfig DeviceConfig_GetWifiConfig();

bool DeviceConfig_SaveWifiConfig(const String& ssid, const String& password);
void DeviceConfig_ClearWifiConfig();

#endif