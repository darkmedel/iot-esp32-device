#pragma once

#include <Arduino.h>

struct HeartbeatData {
  const char* deviceId;
  bool wsConnected;
  size_t eventQueueSize;
};

void Heartbeat_Init();
void Heartbeat_Tick(const HeartbeatData& data);
void Heartbeat_Force();