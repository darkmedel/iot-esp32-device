#include "buttons.h"
#include "config.h"
#include "globals.h"
#include "event_queue.h"

const int buttonPins[BUTTON_COUNT] = { 21, 22, 23, 25, 26, 27 };  //btn1=21 btn2=22 btn3=23 btn4=25 btn5=26 btn6=27

bool lastStableState[BUTTON_COUNT];
bool lastReading[BUTTON_COUNT];
unsigned long lastDebounceTime[BUTTON_COUNT];

unsigned long lastButtonScan = 0;
unsigned long nextMsgId = 1;

void initButtons() {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);

    bool current = digitalRead(buttonPins[i]);
    lastStableState[i] = current;
    lastReading[i] = current;
    lastDebounceTime[i] = 0;
  }

  Serial.println("Buttons initialized");
}

static void generateButtonEvent(int buttonNumber) {
  String eventMsg =
    "BTN|" + deviceId + "|" + String(nextMsgId++) + "|" + String(uptimeSeconds) + "|" + String(buttonNumber);

  enqueueEvent(eventMsg);

  Serial.print("BTN Enqueued: ");
  Serial.println(eventMsg);
}

void loopButtons() {
  if ((millis() - lastButtonScan) < BTN_SCAN_INTERVAL) {
    return;
  }

  lastButtonScan = millis();

  for (int i = 0; i < BUTTON_COUNT; i++) {
    bool reading = digitalRead(buttonPins[i]);

    if (reading != lastReading[i]) {
      lastDebounceTime[i] = millis();
      lastReading[i] = reading;
    }

    if ((millis() - lastDebounceTime[i]) >= BTN_DEBOUNCE) {
      if (reading != lastStableState[i]) {
        lastStableState[i] = reading;

        // botón presionado: con INPUT_PULLUP, LOW = pressed
        if (lastStableState[i] == LOW) {
          int buttonNumber = i + 1;
          generateButtonEvent(buttonNumber);
        }
      }
    }
  }
}