#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>

void initWebSocket();
void loopWebSocket();
void sendStatus();
void handleCommand(const String& incoming);

#endif