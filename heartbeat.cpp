#include "heartbeat.h"
#include "config.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

// ===============================
// ESTADO INTERNO
// ===============================
static unsigned long lastHeartbeat = 0;
static bool forceNow = false;

// ===============================
// HELPERS
// ===============================
static String BuildJson(const HeartbeatData& data) {
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

static bool ShouldSend() {
  if (forceNow) {
    return true;
  }

  unsigned long now = millis();
  return (now - lastHeartbeat) >= HEARTBEAT_INTERVAL;
}

static void MarkAttemptDone() {
  lastHeartbeat = millis();
  forceNow = false;
}

// ===============================
// API
// ===============================
void Heartbeat_Init() {
  lastHeartbeat = millis();
  forceNow = false;
}

void Heartbeat_Tick(const HeartbeatData& data) {
  if (!ShouldSend()) {
    return;
  }

  if (!WiFi.isConnected()) {
    Serial.println("[HB] HEARTBEAT FAILED - WiFi disconnected");
    MarkAttemptDone();
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String payload = BuildJson(data);

  if (!http.begin(client, HEARTBEAT_URL)) {
    Serial.println("[HB] HEARTBEAT FAILED - http.begin error");
    MarkAttemptDone();
    return;
  }

  http.setConnectTimeout(HEARTBEAT_CONNECT_TIMEOUT);
  http.setTimeout(HEARTBEAT_REQUEST_TIMEOUT);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.print("[HB] HEARTBEAT SENT - HTTP ");
    Serial.println(httpCode);
  } else {
    Serial.print("[HB] HEARTBEAT FAILED - HTTP ");
    Serial.println(httpCode);
  }

  http.end();
  MarkAttemptDone();
}

void Heartbeat_Force() {
  forceNow = true;
}