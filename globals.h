#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// Estados globales
extern bool wifiConnected;
extern bool wsConnected;
extern bool cloudReachable;
extern bool otaInProgress;
extern bool wifiConnecting;
extern bool wsInitialized;
extern bool deviceRegistered;
extern bool awaitingBtnAck;

// Identidad
extern String deviceId;
extern String awaitingBtnMsgId;

// Runtime
extern unsigned long uptimeSeconds;

#endif