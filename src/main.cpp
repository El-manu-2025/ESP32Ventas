#include <Arduino.h>
#include "wifi_utils.h"
#include "webserver_utils.h"
#include "api_utils.h"
#include "vibracion.h"

void setup() {
  Serial.begin(115200);
  pinMode(15, OUTPUT);
  


  conectarWifi();
  setupWebServer();
}

void loop() {
  server.handleClient();
  pollAPI();
}
