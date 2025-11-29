#include <Arduino.h>
#include "vibracion.h"


#define VIB_PIN 13

void vibrarPulso(unsigned long ms) {
  digitalWrite(VIB_PIN, HIGH);
  delay(ms);
  digitalWrite(VIB_PIN, LOW);
}

void vibrarUnaVez() {
  Serial.println("Vibrar una vez");
  vibrarPulso(300);
}

void vibrarDosVeces() {
  Serial.println("Vibrar dos veces");
  vibrarPulso(200);
  delay(150);
  vibrarPulso(200);
}

void vibrarErrorPattern(const String &code) {
  Serial.println("Error pattern: " + code);
  int n = code.toInt();
  if (n > 0 && n < 10) {
    for (int i = 0; i < n; i++) {
      vibrarPulso(180);
      delay(150);
    }
  } else {
    for (int i = 0; i < 3; i++) {
      vibrarPulso(300);
      delay(200);
    }
  }
}
