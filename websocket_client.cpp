#include "config.h"
#include "globals.h"
#include "websocket_client.h"
#include "event_queue.h"
#include "heartbeat.h"

#include <WiFi.h>
#include <WebSocketsClient.h>

WebSocketsClient webSocket;

// ===============================
// ESTADO INTERNO WS
// ===============================
static bool socketConnected = false;
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastBtnSendAttempt = 0;
static unsigned long lastPingSentAt = 0;
static unsigned long lastWsActivityAt = 0;
static bool awaitingPong = false;
static uint8_t reconnectFailures = 0;

// ===============================
// HELPERS
// ===============================
static String extractBtnAckMsgId(const String& incoming) {
  const String prefix = "ACK|BTN|";
  if (!incoming.startsWith(prefix)) {
    return "";
  }

  return incoming.substring(prefix.length());
}

static void markWsActivity() {
  lastWsActivityAt = millis();
  awaitingPong = false;
}

static void setWsConnectedState(bool newState, const char* reason) {
  if (wsConnected == newState) {
    return;
  }

  wsConnected = newState;

  Serial.print("[WS] wsConnected = ");
  Serial.print(wsConnected ? "true" : "false");
  Serial.print(" | reason: ");
  Serial.println(reason);

  // Avisar rápido al HeartBeat server ante cambio de estado
  Heartbeat_Force();
}

static void resetWsSessionState() {
  deviceRegistered = false;
  awaitingBtnAck = false;
  awaitingBtnMsgId = "";
  awaitingPong = false;
}

static void evaluateWsHealth() {
  unsigned long now = millis();

  if (!wifiConnected) {
    socketConnected = false;
    resetWsSessionState();
    setWsConnectedState(false, "wifi disconnected");
    return;
  }

  // Sin socket no puede haber salud WS
  if (!socketConnected) {
    setWsConnectedState(false, "socket disconnected");
    return;
  }

  // Aún no registrado en gateway => todavía no se considera canal sano
  if (!deviceRegistered) {
    setWsConnectedState(false, "device not registered");
    return;
  }

  // Timeout de PONG / actividad posterior a PING
  if (awaitingPong && (now - lastPingSentAt) > WS_PONG_TIMEOUT) {
    setWsConnectedState(false, "pong timeout");
    return;
  }

  // Inactividad general del canal
  if ((now - lastWsActivityAt) > WS_INACTIVITY_TIMEOUT) {
    setWsConnectedState(false, "ws inactivity timeout");
    return;
  }

  // Canal sano
  setWsConnectedState(true, "healthy");
}

// ===============================
// EVENTOS WS
// ===============================
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      socketConnected = true;
      reconnectFailures = 0;
      lastWsActivityAt = millis();
      awaitingPong = false;

      Serial.println("WebSocket Connected");
      webSocket.sendTXT("HELLO|" + deviceId);
      evaluateWsHealth();
      break;

    case WStype_DISCONNECTED:
      socketConnected = false;
      resetWsSessionState();

      Serial.println("WebSocket Disconnected");
      evaluateWsHealth();
      break;

    case WStype_PONG:
      Serial.println("WS PONG received");
      markWsActivity();
      evaluateWsHealth();
      break;

    case WStype_TEXT:
      {
        String incoming = String((char*)payload);

        Serial.print("WS Message: ");
        Serial.println(incoming);

        markWsActivity();

        if (incoming == "ACK|HELLO|" + deviceId) {
          deviceRegistered = true;
          Serial.println("Device registered on Gateway");
        } else if (incoming == "PONG") {
          Serial.println("App-level PONG received");
        } else if (incoming == "ACK|STATUS") {
          // Señal válida de actividad del Gateway
        }

        String ackMsgId = extractBtnAckMsgId(incoming);
        if (ackMsgId.length() > 0) {
          if (awaitingBtnAck && ackMsgId == awaitingBtnMsgId) {
            if (ackEvent()) {
              Serial.print("BTN ACK confirmed, removed from queue. MsgId=");
              Serial.println(ackMsgId);
            }

            awaitingBtnAck = false;
            awaitingBtnMsgId = "";
          }
        }

        handleCommand(incoming);
        evaluateWsHealth();
        break;
      }

    case WStype_ERROR:
      Serial.println("WebSocket Error");
      break;

    default:
      break;
  }
}

// ===============================
// INICIALIZACIÓN
// ===============================
void initWebSocket() {
  Serial.println("Initializing WebSocket...");

  socketConnected = false;
  setWsConnectedState(false, "initializing");
  resetWsSessionState();

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);

  lastReconnectAttempt = millis();
  lastWsActivityAt = millis();
}

// ===============================
// STATUS
// ===============================
void sendStatus() {
  if (!wsConnected || !deviceRegistered) {
    return;
  }

  String msg = "STATUS|" + deviceId + "|FW:" + String(FW_VERSION) + "|RSSI:" + String(WiFi.RSSI()) + "|UP:" + String(uptimeSeconds) + "|WS:" + String(wsConnected ? 1 : 0) + "|EVQ:" + String(eventQueueSize());

  webSocket.sendTXT(msg);

  Serial.print("STATUS Sent: ");
  Serial.println(msg);
}

// ===============================
// LOOP PRINCIPAL WS
// ===============================
void loopWebSocket() {
  static unsigned long lastPing = 0;
  static unsigned long lastStatus = 0;

  if (!wifiConnected) {
    socketConnected = false;
    wsInitialized = false;
    resetWsSessionState();
    evaluateWsHealth();
    return;
  }

  webSocket.loop();

  // Reevaluar continuamente salud del canal
  evaluateWsHealth();

  // Enviar un BTN pendiente solo si el canal está realmente sano
  if (wsConnected && deviceRegistered && !awaitingBtnAck) {
    String queuedMsg;
    if (peekEvent(queuedMsg)) {
      webSocket.sendTXT(queuedMsg);

      Serial.print("Event Sent (awaiting ACK): ");
      Serial.println(queuedMsg);

      // BTN|DEVICEID|MSGID|TIMESTAMP|BUTTON
      int p1 = queuedMsg.indexOf('|');
      int p2 = queuedMsg.indexOf('|', p1 + 1);
      int p3 = queuedMsg.indexOf('|', p2 + 1);

      if (p2 > 0 && p3 > p2) {
        awaitingBtnMsgId = queuedMsg.substring(p2 + 1, p3);
        awaitingBtnAck = true;
        lastBtnSendAttempt = millis();

        Serial.print("Waiting BTN ACK for MsgId=");
        Serial.println(awaitingBtnMsgId);
      }
    }
  }

  // Reintento del mismo BTN si no llegó ACK
  if (wsConnected && deviceRegistered && awaitingBtnAck && (millis() - lastBtnSendAttempt) >= WS_RECONNECT_INTERVAL) {
    String queuedMsg;
    if (peekEvent(queuedMsg)) {
      webSocket.sendTXT(queuedMsg);
      lastBtnSendAttempt = millis();

      Serial.print("Re-sent pending BTN: ");
      Serial.println(queuedMsg);
    }
  }

  // PING lógico a nivel aplicación
  if (socketConnected && deviceRegistered && (millis() - lastPing) >= WS_PING_INTERVAL) {
    webSocket.sendTXT("PING");
    lastPing = millis();
    lastPingSentAt = millis();
    awaitingPong = true;

    Serial.println("WS PING sent");
  }

  // STATUS periódico
  if (wsConnected && deviceRegistered && (millis() - lastStatus) >= STATUS_INTERVAL) {
    sendStatus();
    lastStatus = millis();
  }

  // Reintento de reconexión si no hay socket conectado
  if (!socketConnected && (millis() - lastReconnectAttempt) >= WS_RECONNECT_INTERVAL) {
    Serial.println("WebSocket reconnect attempt...");

    webSocket.disconnect();
    webSocket.begin(WS_HOST, WS_PORT, WS_PATH);

    lastReconnectAttempt = millis();

    if (reconnectFailures < 255) {
      reconnectFailures++;
    }

    Serial.print("[WS] reconnectFailures=");
    Serial.println(reconnectFailures);

    if (reconnectFailures >= WS_MAX_RECONNECT_FAILURES) {
      setWsConnectedState(false, "multiple reconnect failures");
    }
  }
}

// ===============================
// COMANDOS
// ===============================
void handleCommand(const String& incoming) {
  if (!incoming.startsWith("CMD|")) {
    return;
  }

  if (incoming == "CMD|STATUS") {
    Serial.println("Command received: STATUS -> sending immediate status");
    sendStatus();
    return;
  }

  if (incoming == "CMD|REBOOT") {
    Serial.println("Command received: REBOOT");

    if (socketConnected) {
      webSocket.sendTXT("ACK|CMD|REBOOT");
      delay(200);
    }

    ESP.restart();
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(incoming);
}