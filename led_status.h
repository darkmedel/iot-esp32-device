#pragma once

#include <Arduino.h>

enum LedStatus
{
  LED_BOOT,
  LED_CONFIG_PORTAL,
  LED_WIFI_CONNECTING,
  LED_WIFI_CONNECTED,
  LED_WS_CONNECTED,
  LED_DEGRADED,
  LED_OFFLINE
};

void LedStatus_Init(uint8_t pin);
void LedStatus_Set(LedStatus status);
void LedStatus_Tick();