#include <WiFi.h>
#include "wifi_utils.h"

const char* WIFI_SSID = "Manuel";
const char* WIFI_PASS = "22053605";

void conectarWifi() {
  Serial.printf("Conectando a WiFi %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado.");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo se pudo conectar al WiFi.");
  }
}
