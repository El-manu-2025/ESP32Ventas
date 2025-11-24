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
    bool lastSaleSuccess = false;
    String saleRawJson = "";
    String lastError = "";
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



#endif 
