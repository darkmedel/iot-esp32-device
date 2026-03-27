#include "config.h"
#include "logger.h"
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
static bool lastSocketConnectedLogged = false;

// ===============================
// HELPERS
// ===============================
static String extractBtnAckMsgId(const String& incoming)
{
  const String prefix = "ACK|BTN|";
  if (!incoming.startsWith(prefix))
  {
    return "";
  }

  return incoming.substring(prefix.length());
}

static void markWsActivity()
{
  lastWsActivityAt = millis();
  awaitingPong = false;
}

static void setWsConnectedState(bool newState, const char* reason)
{
  if (wsConnected == newState)
  {
    return;
  }

  wsConnected = newState;

  if (wsConnected)
  {
    String msg = "[WS] Healthy: ";
    msg += reason;
    LOG_INFO(msg);
  }
  else
  {
    String msg = "[WS] Degraded/Disconnected: ";
    msg += reason;
    LOG_WARN(msg);
  }

  Heartbeat_Force();
}

static void resetWsSessionState()
{
  deviceRegistered = false;
  awaitingBtnAck = false;
  awaitingBtnMsgId = "";
  awaitingPong = false;
}

static void evaluateWsHealth()
{
  unsigned long now = millis();

  if (!wifiConnected)
  {
    socketConnected = false;
    resetWsSessionState();
    setWsConnectedState(false, "wifi disconnected");
    return;
  }

  if (!socketConnected)
  {
    setWsConnectedState(false, "socket disconnected");
    return;
  }

  if (!deviceRegistered)
  {
    setWsConnectedState(false, "device not registered");
    return;
  }

  if (awaitingPong && (now - lastPingSentAt) > WS_PONG_TIMEOUT)
  {
    setWsConnectedState(false, "pong timeout");
    return;
  }

  if ((now - lastWsActivityAt) > WS_INACTIVITY_TIMEOUT)
  {
    setWsConnectedState(false, "ws inactivity timeout");
    return;
  }

  setWsConnectedState(true, "healthy");
}

// ===============================
// EVENTOS WS
// ===============================
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
  switch (type)
  {
    case WStype_CONNECTED:
      socketConnected = true;
      reconnectFailures = 0;
      lastWsActivityAt = millis();
      awaitingPong = false;

      LOG_INFO("[WS] Socket connected");
      webSocket.sendTXT("HELLO|" + deviceId);
      evaluateWsHealth();
      break;

    case WStype_DISCONNECTED:
      socketConnected = false;
      resetWsSessionState();

      LOG_WARN("[WS] Socket disconnected");
      evaluateWsHealth();
      break;

    case WStype_PONG:
      markWsActivity();
#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
      LOG_DEBUG("[WS] PONG received");
#endif
      evaluateWsHealth();
      break;

    case WStype_TEXT:
    {
      String incoming = String((char*)payload);
      markWsActivity();

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
      LOG_DEBUGF("[WS] Message: ", incoming);
#endif

      if (incoming == "ACK|HELLO|" + deviceId)
      {
        deviceRegistered = true;
        LOG_INFO("[WS] Device registered on Gateway");
      }
      else if (incoming == "PONG")
      {
#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
        LOG_DEBUG("[WS] App-level PONG received");
#endif
      }
      else if (incoming == "ACK|STATUS")
      {
        // Actividad válida del Gateway; no log en operación normal.
      }

      String ackMsgId = extractBtnAckMsgId(incoming);
      if (ackMsgId.length() > 0)
      {
        if (awaitingBtnAck && ackMsgId == awaitingBtnMsgId)
        {
          if (ackEvent())
          {
#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
            LOG_DEBUGF("[WS] BTN ACK confirmed. MsgId=", ackMsgId);
#endif
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
      LOG_ERROR("[WS] WebSocket error");
      break;

    default:
      break;
  }
}

// ===============================
// INICIALIZACIÓN
// ===============================
void initWebSocket()
{
  socketConnected = false;
  setWsConnectedState(false, "initializing");
  resetWsSessionState();

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);

  lastReconnectAttempt = millis();
  lastWsActivityAt = millis();
  lastSocketConnectedLogged = false;
}

// ===============================
// STATUS
// ===============================
void sendStatus()
{
  if (!wsConnected || !deviceRegistered)
  {
    return;
  }

  String msg = "STATUS|" + deviceId +
               "|FW:" + String(FW_VERSION) +
               "|RSSI:" + String(WiFi.RSSI()) +
               "|UP:" + String(uptimeSeconds) +
               "|WS:" + String(wsConnected ? 1 : 0) +
               "|EVQ:" + String(eventQueueSize());

  webSocket.sendTXT(msg);

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
  LOG_DEBUGF("[WS] STATUS sent: ", msg);
#endif
}

// ===============================
// LOOP PRINCIPAL WS
// ===============================
void loopWebSocket()
{
  static unsigned long lastPing = 0;
  static unsigned long lastStatus = 0;

  if (!wifiConnected)
  {
    socketConnected = false;
    wsInitialized = false;
    resetWsSessionState();
    evaluateWsHealth();
    return;
  }

  webSocket.loop();
  evaluateWsHealth();

  if (wsConnected && deviceRegistered && !awaitingBtnAck)
  {
    String queuedMsg;
    if (peekEvent(queuedMsg))
    {
      webSocket.sendTXT(queuedMsg);

      int p1 = queuedMsg.indexOf('|');
      int p2 = queuedMsg.indexOf('|', p1 + 1);
      int p3 = queuedMsg.indexOf('|', p2 + 1);

      if (p2 > 0 && p3 > p2)
      {
        awaitingBtnMsgId = queuedMsg.substring(p2 + 1, p3);
        awaitingBtnAck = true;
        lastBtnSendAttempt = millis();

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
        LOG_DEBUGF("[WS] BTN sent, awaiting ACK. MsgId=", awaitingBtnMsgId);
#endif
      }
    }
  }

  if (wsConnected && deviceRegistered && awaitingBtnAck &&
      (millis() - lastBtnSendAttempt) >= WS_RECONNECT_INTERVAL)
  {
    String queuedMsg;
    if (peekEvent(queuedMsg))
    {
      webSocket.sendTXT(queuedMsg);
      lastBtnSendAttempt = millis();

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
      LOG_DEBUGF("[WS] Re-sent pending BTN: ", queuedMsg);
#endif
    }
  }

  if (socketConnected && deviceRegistered && (millis() - lastPing) >= WS_PING_INTERVAL)
  {
    webSocket.sendTXT("PING");
    lastPing = millis();
    lastPingSentAt = millis();
    awaitingPong = true;

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DEBUG("[WS] PING sent");
#endif
  }

  if (wsConnected && deviceRegistered && (millis() - lastStatus) >= STATUS_INTERVAL)
  {
    sendStatus();
    lastStatus = millis();
  }

  if (!socketConnected && (millis() - lastReconnectAttempt) >= WS_RECONNECT_INTERVAL)
  {
      LOG_WARN("[WS] Reconnect attempt...");

      webSocket.disconnect();
      webSocket.begin(WS_HOST, WS_PORT, WS_PATH);

      lastReconnectAttempt = millis();

      if (reconnectFailures < 255)
      {
        reconnectFailures++;
      }

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
      LOG_DEBUGF("[WS] reconnectFailures=", reconnectFailures);
#endif

      if (reconnectFailures >= WS_MAX_RECONNECT_FAILURES)
      {
        setWsConnectedState(false, "multiple reconnect failures");
      }
  }
}

// ===============================
// COMANDOS
// ===============================
void handleCommand(const String& incoming)
{
  if (!incoming.startsWith("CMD|"))
  {
    return;
  }

  if (incoming == "CMD|STATUS")
  {
    LOG_INFO("[WS] Command received: STATUS");
    sendStatus();
    return;
  }

  if (incoming == "CMD|REBOOT")
  {
    LOG_WARN("[WS] Command received: REBOOT");

    if (socketConnected)
    {
      webSocket.sendTXT("ACK|CMD|REBOOT");
      delay(200);
    }

    ESP.restart();
    return;
  }

  LOG_WARNF("[WS] Unknown command: ", incoming);
}