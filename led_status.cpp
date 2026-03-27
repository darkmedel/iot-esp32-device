#include "led_status.h"

static uint8_t g_ledPin = 2;
static LedStatus g_currentStatus = LED_BOOT;

static unsigned long g_lastToggle = 0;
static bool g_ledState = false;

static uint32_t GetInterval(LedStatus status)
{
  switch (status)
  {
    case LED_BOOT:
      return 150;

    case LED_CONFIG_PORTAL:
      return 500;

    case LED_WIFI_CONNECTING:
      return 300;

    case LED_WIFI_CONNECTED:
      return 800;

    case LED_DEGRADED:
      return 200;

    case LED_OFFLINE:
      return 1500;

    case LED_WS_CONNECTED:
    default:
      return 1000;
  }
}

void LedStatus_Init(uint8_t pin)
{
  g_ledPin = pin;
  pinMode(g_ledPin, OUTPUT);
  digitalWrite(g_ledPin, LOW);
  g_ledState = false;
  g_lastToggle = millis();
}

void LedStatus_Set(LedStatus status)
{
  g_currentStatus = status;
}

void LedStatus_Tick()
{
  if (g_currentStatus == LED_WS_CONNECTED)
  {
    g_ledState = true;
    digitalWrite(g_ledPin, HIGH);
    return;
  }

  unsigned long now = millis();
  uint32_t interval = GetInterval(g_currentStatus);

  if ((now - g_lastToggle) >= interval)
  {
    g_lastToggle = now;
    g_ledState = !g_ledState;
    digitalWrite(g_ledPin, g_ledState ? HIGH : LOW);
  }
}