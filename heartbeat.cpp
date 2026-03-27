#include "heartbeat.h"
#include "config.h"
#include "logger.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

// ===============================
// ESTADO INTERNO
// ===============================
static unsigned long lastHeartbeat = 0;
static bool forceNow = false;

static bool lastSendFailed = false;
static int lastHttpCode = 0;
static unsigned long lastFailureLogAt = 0;

// ===============================
// HELPERS
// ===============================
static String BuildJson(const HeartbeatData& data)
{
  String json;
  json.reserve(220);

  json += "{";
  json += "\"deviceId\":\"";
  json += data.deviceId;
  json += "\",";
  json += "\"uptime\":";
  json += String(millis() / 1000UL);
  json += ",";
  json += "\"rssi\":";
  json += String(WiFi.isConnected() ? WiFi.RSSI() : -127);
  json += ",";
  json += "\"wsConnected\":";
  json += data.wsConnected ? "true" : "false";
  json += ",";
  json += "\"eventQueueSize\":";
  json += String((unsigned int)data.eventQueueSize);
  json += ",";
  json += "\"freeHeap\":";
  json += String(ESP.getFreeHeap());
  json += "}";

  return json;
}

static bool ShouldSend()
{
  if (forceNow)
  {
    return true;
  }

  unsigned long now = millis();
  return (now - lastHeartbeat) >= HEARTBEAT_INTERVAL;
}

static void MarkAttemptDone()
{
  lastHeartbeat = millis();
  forceNow = false;
}

static void LogFailureThrottled(const String& message)
{
  unsigned long now = millis();

  if (!lastSendFailed || (now - lastFailureLogAt) >= HEARTBEAT_FAIL_LOG_INTERVAL)
  {
    LOG_WARN(message);
    lastFailureLogAt = now;
  }

  lastSendFailed = true;
}

static void MarkRecoveredIfNeeded(int httpCode)
{
  if (lastSendFailed)
  {
    String msg = "[HB] Recovered. HTTP ";
    msg += httpCode;
    LOG_INFO(msg);
  }

  lastSendFailed = false;
  lastHttpCode = httpCode;
}

// ===============================
// API
// ===============================
void Heartbeat_Init()
{
  lastHeartbeat = millis();
  forceNow = false;
  lastSendFailed = false;
  lastHttpCode = 0;
  lastFailureLogAt = 0;
}

void Heartbeat_Tick(const HeartbeatData& data)
{
  if (!ShouldSend())
  {
    return;
  }

  if (!WiFi.isConnected())
  {
    LogFailureThrottled("[HB] Skipped: WiFi disconnected");
    MarkAttemptDone();
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String payload = BuildJson(data);

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
  LOG_DEBUGF("[HB] payload=", payload);
#endif

  if (!http.begin(client, HEARTBEAT_URL))
  {
    LogFailureThrottled("[HB] Failed: http.begin error");
    MarkAttemptDone();
    return;
  }

  http.setConnectTimeout(HEARTBEAT_CONNECT_TIMEOUT);
  http.setTimeout(HEARTBEAT_REQUEST_TIMEOUT);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  http.end();

  if (httpCode > 0)
  {
    MarkRecoveredIfNeeded(httpCode);
  }
  else
  {
    String msg = "[HB] Failed. HTTP ";
    msg += httpCode;
    LogFailureThrottled(msg);
    lastHttpCode = httpCode;
  }

  MarkAttemptDone();
}

void Heartbeat_Force()
{
  forceNow = true;
}