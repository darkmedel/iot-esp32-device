#include "event_queue.h"
#include "config.h"

EventItem queueBuffer[MAX_EVENT_QUEUE];

int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

void enqueueEvent(const String& msg) {
  if (queueCount == MAX_EVENT_QUEUE) {
    // descarta el más antiguo
    queueHead = (queueHead + 1) % MAX_EVENT_QUEUE;
    queueCount--;
  }

  queueBuffer[queueTail].payload = msg;
  queueTail = (queueTail + 1) % MAX_EVENT_QUEUE;
  queueCount++;
}

bool peekEvent(String& msg) {
  if (queueCount == 0) {
    return false;
  }

  msg = queueBuffer[queueHead].payload;
  return true;
}

bool ackEvent() {
  if (queueCount == 0) {
    return false;
  }

  queueHead = (queueHead + 1) % MAX_EVENT_QUEUE;
  queueCount--;
  return true;
}

size_t eventQueueSize() {
  return (size_t)queueCount;
}