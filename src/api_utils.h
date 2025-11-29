#ifndef API_UTILS_H
#define API_UTILS_H

#include <Arduino.h>

// Estructura usada por otros módulos
struct Estado {
    long saleId = 0;
    long lastProductId = 0; 
    String lastProductName; 
    String lastProductCodigo = "";
    int lastProductStock = 0;
    // cantidad vendida en la última venta (separada del stock real)
    int lastProductSold = 0;
    bool lastSaleSuccess = false;
    String saleRawJson = "";
    String lastError = "";
    // modo: 0 = normal, 1 = LED only, 2 = silent (no alerts)
    int mode = 0;
    // último código o nombre de botón IR recibido (por ejemplo: "UP", "OK", "POWER")
    String lastIR = "";
    // secuencia/contador que incrementa en cada evento IR para detectar pulsaciones repetidas
    unsigned long lastIRSeq = 0;
    // (Historial de alertas eliminado)
};

extern Estado estado;
extern long lastSaleId;

// JWT
extern String jwtToken;
extern unsigned long tokenObtainedAt;

bool tokenValido();
bool obtainToken();
bool pollAPI();
bool consultarEndpointVentas(const String &url);
bool obtenerProducto(const String &productoUrl, String &nombreProducto, long &productoCodigo, long &productoId);

// (pushAlert removed)

extern void (*vibUna)();
extern void (*vibDos)();
extern void (*vibErr)(const String &code);


#endif 
