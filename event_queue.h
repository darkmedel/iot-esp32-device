#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <Arduino.h>

struct EventItem {
  String payload;
};

void enqueueEvent(const String& msg);
bool peekEvent(String& msg);
bool ackEvent();
size_t eventQueueSize();

#endif