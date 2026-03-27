#include <Arduino.h>
#include "config.h"

bool IsConfigResetRequested() {
  pinMode(CONFIG_RESET_BUTTON_PIN, INPUT_PULLUP);

  if (digitalRead(CONFIG_RESET_BUTTON_PIN) != LOW) {
    return false;
  }

  Serial.println("Reset button detected. Hold for 10 seconds to clear config...");

  unsigned long start = millis();
  bool ledState = false;
  unsigned long lastBlink = 0;

  while ((millis() - start) < CONFIG_RESET_HOLD_MS) {
    if (digitalRead(CONFIG_RESET_BUTTON_PIN) != LOW) {
      Serial.println("Reset cancelled");
      digitalWrite(LED_PIN, LOW);
      return false;
    }

    if ((millis() - lastBlink) >= 200UL) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastBlink = millis();
    }

    delay(20);
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println("Reset confirmed");
  return true;
}