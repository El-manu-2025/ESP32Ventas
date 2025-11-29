// main.cpp
#include <Arduino.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRremoteESP8266.h>
#include "wifi_utils.h"
#include "webserver_utils.h"
#include "api_utils.h"
#include "vibracion.h"

// PINs
constexpr uint8_t VIB_PIN = 13;   // motor de vibración
constexpr uint8_t LED_PIN = 2;    // LED integrado 
constexpr uint8_t IR_PIN  = 4;    // receptor IR (cambiado a GPIO4)

// IR
IRrecv irrecv(IR_PIN);
decode_results results;

// Modo de alertas
// 0 = normal (vibrar + sonido en celular),
// 1 = LED only (no vibrar, no sonidos desde la UI),
// 2 = silent (no vibrar, no LED, no sonidos)
volatile int alertMode = 0;

// Debounce para IR (evitar toggles múltiples)
unsigned long lastIrMillis = 0;
uint64_t lastIrValue = 0;
const unsigned long IR_DEBOUNCE_MS = 500;

// Led flash control (no bloquear)
unsigned long ledFlashUntil = 0;

// Parámetros de parpadeo (opción B: 300ms ON / 300ms OFF)
const unsigned long BLINK_ON_MS  = 300;
const unsigned long BLINK_OFF_MS = 300;

// Prototipos de notificador (implementados aquí y usados por api_utils.cpp)
void notifySaleOnce();
void notifyStockZero();
void notifyProductInvalid();

// Mapeo y manejo de códigos IR. Devuelve true si el código es RECONOCIDO
bool handleIrCode(uint64_t value);

// Funciones auxiliares
void ledPulse(unsigned long onMs) {
  digitalWrite(LED_PIN, HIGH);
  delay(onMs);
  digitalWrite(LED_PIN, LOW);
}

// Notificadores usados por api_utils.cpp
void notifySaleOnce() {
  if (alertMode == 2) {
    // silencio total: no hacer nada
    return;
  }
  if (alertMode == 1) {
    // LED 1 pulso (venta nueva)
    ledPulse(BLINK_ON_MS);
  } else {
    // modo normal: vibrar
    vibrarUnaVez();
  }
}

void notifyStockZero() {
  if (alertMode == 2) {
    return;
  }
  if (alertMode == 1) {
    ledPulse(BLINK_ON_MS);
    delay(150);
    ledPulse(BLINK_ON_MS);
  } else {
    vibrarDosVeces();
  }
}

void notifyProductInvalid() {
  if (alertMode == 2) {
    return;
  }
  if (alertMode == 1) {
    ledPulse(BLINK_ON_MS);
    delay(150);
    ledPulse(BLINK_ON_MS);
  } else {
    vibrarDosVeces();
  }
}

// Cambiar modo visualmente (llamado desde handler IR)
void setAlertMode(int mode) {
  alertMode = mode;
  estado.mode = mode;
  Serial.printf("Alert mode set to %d\n", mode);

    if (alertMode == 2) {
    digitalWrite(LED_PIN, LOW);
  }
}

// Parpadeo breve usado por cualquier botón (si no esta el modo silent)
void flashLedForButton() {
  if (alertMode == 2) return;
  // No bloquear: encender LED y programar apagado en loop
  digitalWrite(LED_PIN, HIGH);
  const unsigned long FLASH_MS = 120;
  // Guardamos el tiempo hasta el cual debe mantenerse encendido
  extern unsigned long ledFlashUntil;
  ledFlashUntil = millis() + FLASH_MS;
}

// Manejar códigos IR específicos
bool handleIrCode(uint64_t value) {
  // Mapear a cadenas legibles y acciones mínimas
  String name = "";
  switch (value) {
    case 0xE0E040BFULL: // ON/OFF
      name = "POWER";
      // Toggle LED-only: si ya está en modo SOLOLED vuelve a Normal (0)
      if (alertMode == 1) setAlertMode(0);
      else setAlertMode(1);
      break;
    case 0xE0E0F00FULL: // SILENCIO
      name = "SILENCE";
      // Toggle Silence: si ya está en modo SILENT vuelve a Normal (0)
      if (alertMode == 2) setAlertMode(0);
      else setAlertMode(2);
      break;
  
    default:
      return false;
  }

  estado.lastIR = name;
  
  estado.lastIRSeq++;
  return true;
}

void setup() {
  Serial.begin(115200);

  // Pines
  pinMode(VIB_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // IR
  irrecv.enableIRIn();
  Serial.printf("IR receiver on pin %d\n", IR_PIN);

  // Conectamos el WiFi y el servidor
  conectarWifi();
  setupWebServer();

  // Crear tarea en segundo plano para hacer poll a la API sin bloquear el loop principal
  auto pollTask = [](void* arg) {
    (void)arg;
    Serial.println("PollAPI task started");
    for(;;) {
      if (WiFi.status() == WL_CONNECTED) {
        pollAPI();
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
    }

  };

  // Intentar crear la tarea con más stack para evitar stack overflow durante operaciones HTTPS/JSON
  BaseType_t res = xTaskCreatePinnedToCore(pollTask, "PollAPI", 8192, NULL, 1, NULL, 1);
  if (res != pdPASS) {
    Serial.println("WARNING: no se pudo crear PollAPI task, se usará fallback en loop");
  }

  // pequeña señal de inicio
  for (int i = 0; i < 2; ++i) {
    digitalWrite(LED_PIN, HIGH);
    delay(80);
    digitalWrite(LED_PIN, LOW);
    delay(80);
  }
}

void loop() {
  // Manejo cliente web
  server.handleClient();

  // Leer IR 
  if (irrecv.decode(&results)) {
    uint64_t value = results.value;
    unsigned long now = millis();

    // Aqui ignoramos repetidos cortos
    if (value != 0 && (value != lastIrValue || (now - lastIrMillis) > IR_DEBOUNCE_MS)) {
      Serial.printf("IR recibido: 0x%llX\n", (unsigned long long)value);
      lastIrValue = value;
      lastIrMillis = now;

      // Manejar código específico
      if (handleIrCode(value)) {
        flashLedForButton();
      } else {
        Serial.printf("IR no reconocido (ignorado): 0x%llX\n", (unsigned long long)value);
      }
    } else {
      // repetido: lo ignoramos
    }

    irrecv.resume(); // listo para el siguiente
  }

  // Control no bloqueante del LED flash
  unsigned long now = millis();
  if (ledFlashUntil != 0 && now >= ledFlashUntil) {
    digitalWrite(LED_PIN, LOW);
    ledFlashUntil = 0;
  }

  // Epera para evitar saturación del loop
  delay(10);
}
