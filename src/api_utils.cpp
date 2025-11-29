// api_utils.cpp
#include "api_utils.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>

#include "vibracion.h"
#include "config.h"

// notificadores implementados en main.cpp
extern void notifySaleOnce();
extern void notifyStockZero();
extern void notifyProductInvalid();

// ===========================
// VARIABLES GLOBALES
// ===========================
String jwtToken = "";
unsigned long tokenObtainedAt = 0;
bool yaVibroSinStock = false;
bool yaVibroProductoInvalido = false;

// Declarados en config.cpp
extern const String API_BASE;
extern const String TOKEN_ENDPOINT;
extern const String VENTAS_LATEST_ENDPOINT;

// Estructura Estado
extern Estado estado;
extern long lastSaleId; 

// ===========================
// TOKEN TTL
// ===========================
static const unsigned long TOKEN_REAL_TTL_MS = 15UL * 60UL * 1000UL;
static const unsigned long TOKEN_MARGIN_MS   = 3UL * 60UL * 1000UL;

static inline bool timeout_passed(unsigned long start, unsigned long ms) {
    return (millis() - start) >= ms;
}

// ==========================================================
//   OBTENER PRODUCTO
// ==========================================================
bool obtenerProducto(const String &productoUrl, String &nombreProducto, long &productoCodigo, long &productoId, int &stockProducto ) {

    if (jwtToken.isEmpty()) {
        estado.lastError = "Producto: sin token";
        return false;
    }

    // Convertir http → https 
    String httpsUrl = productoUrl;
    httpsUrl.replace("http://", "https://");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, httpsUrl)) {
        estado.lastError = "Producto begin fail";
        return false;
    }

    http.addHeader("Authorization", "Bearer " + jwtToken);
    http.addHeader("Accept", "application/json");
    http.setTimeout(3000);

    int code = http.GET();

    // Token expiró
    if (code == 401) {
        http.end();
        obtainToken();
        estado.lastError = "Producto HTTP 401";
        return false;
    }

    if (code != 200) {
        estado.lastError = "Producto HTTP " + String(code);
        http.end();
        return false;
    }

    String resp = http.getString();
    DynamicJsonDocument doc(2048);

    if (deserializeJson(doc, resp)) {
        estado.lastError = "Parse producto";
        http.end();
        return false;
    }

    if (doc["nombre"].isNull()) {
        estado.lastError = "Producto sin nombre";
        http.end();
        return false;
    }

    // ------------------------------------------------------
    // EXTRAER DATOS DEL PRODUCTO
    // ------------------------------------------------------
    nombreProducto = doc["nombre"].as<String>();

    // Código del producto
    productoCodigo = doc["codigo"].is<int>() ? doc["codigo"].as<long>() : String(doc["codigo"].as<const char*>()).toInt();

    // Extraer el ID usando la URL
    String urlCompleta = doc["url"].as<String>();   
    int pos1 = urlCompleta.lastIndexOf('/', urlCompleta.length() - 2);
    int pos2 = urlCompleta.lastIndexOf('/');
    String idStr = urlCompleta.substring(pos1 + 1, pos2);
    productoId = idStr.toInt(); 

    // STOCK: puede se llama "cantidad" = "stock" segun mi API — intento ambos
    if (doc.containsKey("stock")) {
        stockProducto = doc["stock"] | 0;
    } else if (doc.containsKey("cantidad")) {
        stockProducto = doc["cantidad"] | 0;
    } else {
        stockProducto = 0;
    }

    http.end();
    return true;
}



bool obtenerProducto(const String &productoUrl, String &nombreProducto, long &productoCodigo, long &productoId) {
    int dummyStock = 0;
    return obtenerProducto(productoUrl, nombreProducto, productoCodigo, productoId, dummyStock);
}

// ==========================================================
//   POLL
// ==========================================================
bool pollAPI() {

    if (WiFi.status() != WL_CONNECTED) {
        estado.lastError = "WiFi perdido";
        return false;
    }

    if (!tokenValido()) {
        if (!obtainToken()) return false;
    }

    return consultarEndpointVentas(API_BASE + VENTAS_LATEST_ENDPOINT);
}

// ==========================================================
//   CONSULTAR ÚLTIMA VENTA
// ==========================================================
bool consultarEndpointVentas(const String &url) {

    if (jwtToken.isEmpty()) {
        estado.lastError = "Sin token";
        return false;
    }

    std::function<bool(bool)> doRequest;

    doRequest = [&](bool allowRetry) -> bool {

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        if (!http.begin(client, url)) {
            estado.lastError = "Ventas begin fail";
            return false;
        }

        http.addHeader("Authorization", "Bearer " + jwtToken);
        http.addHeader("Accept", "application/json");
        http.setTimeout(3000);

        int code = http.GET();

        if (code == 401) {
            http.end();
            if (allowRetry && obtainToken()) {
                return doRequest(false);
            }
            estado.lastError = "Ventas HTTP 401";
            return false;
        }

        if (code != 200) {
            estado.lastError = "Ventas HTTP " + String(code);
            http.end();
            return false;
        }

        String resp = http.getString();
        DynamicJsonDocument doc(4096);

        if (deserializeJson(doc, resp)) {
            estado.lastError = "Parse ventas";
            http.end();
            return false;
        }

        if (!doc["results"].is<JsonArray>()) {
            estado.lastError = "No results[]";
            http.end();
            return false;
        }

        JsonArray arr = doc["results"].as<JsonArray>();
        if (arr.size() == 0) {
            estado.lastError = "";
            http.end();
            return true;
        }

        JsonObject venta = arr[0];

        // ===== EXTRAER ID ====
        String ventaUrl = venta["url"].as<String>();
        ventaUrl.trim();
        if (ventaUrl.endsWith("/")) ventaUrl.remove(ventaUrl.length() - 1);

        int lastSlash = ventaUrl.lastIndexOf('/');
        if (lastSlash < 0) {
            estado.lastError = "URL venta inválida";
            http.end();
            return false;
        }

        long saleIdLong = ventaUrl.substring(lastSlash + 1).toInt();
        if (saleIdLong <= 0) {
            estado.lastError = "SaleId no numérico";
            http.end();
            return false;
        }

        // Notificar nueva venta
        if (saleIdLong != lastSaleId) {
            notifySaleOnce();
        }

        lastSaleId = saleIdLong;

        estado.saleId = saleIdLong;
        estado.saleRawJson = resp;
        estado.lastSaleSuccess = true;

        // ===== DETALLES DE PRODUCTO ====
        if (venta["detalles"].is<JsonArray>() && venta["detalles"].size() > 0) {

            JsonObject det = venta["detalles"][0];

            String productoUrl = det["producto"].as<String>();
            int cantidadVendida = det["cantidad"] | 0;

            estado.lastProductSold = cantidadVendida;

            long urlProductoId = -1;
            {
                String pUrl = productoUrl;
                pUrl.trim();
                if (pUrl.endsWith("/")) pUrl.remove(pUrl.length() - 1);
                int slash = pUrl.lastIndexOf('/');
                if (slash > 0) urlProductoId = pUrl.substring(slash + 1).toInt();
            }

            String nombreProducto;
            long codigoProducto = -1;
            long productoId = 0;
            int stockReal = 0; 

            if (obtenerProducto(productoUrl, nombreProducto, codigoProducto, productoId, stockReal)) {

                estado.lastProductCodigo = String(codigoProducto);
                estado.lastProductId     = productoId;
                estado.lastProductName   = nombreProducto;
                estado.lastProductStock  = stockReal;

                if (estado.lastProductStock <= 0) {
                    if (!yaVibroSinStock) {
                        notifyStockZero();
                        yaVibroSinStock = true;
                    }
                } else {
                    yaVibroSinStock = false;
                }

        
                yaVibroProductoInvalido = false;

            } else {
                estado.lastProductCodigo = "Desconocido";
                estado.lastProductId = (urlProductoId > 0 ? urlProductoId : -1);

                if (!yaVibroProductoInvalido) {
                    notifyProductInvalid();
                    yaVibroProductoInvalido = true;
                }
            }

        } else {
            estado.lastProductCodigo = "";
            estado.lastProductStock = 0;
            estado.lastProductSold = 0;
            estado.lastProductId = -1;
        }

        estado.lastError = "";
        http.end();
        return true;
    };

    return doRequest(true);
}

// ==========================================================
//   🔥 FUNCIÓN tokenValido()
// ==========================================================
bool tokenValido() {

    if (jwtToken.isEmpty()) return false;

    unsigned long elapsed = millis() - tokenObtainedAt;

    unsigned long safeTTL = TOKEN_REAL_TTL_MS - TOKEN_MARGIN_MS;

    return elapsed < safeTTL;
}

// ==========================================================
//   🔥 FUNCIÓN obtainToken()
// ==========================================================
bool obtainToken() {

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    String url = API_BASE + TOKEN_ENDPOINT;

    if (!http.begin(client, url)) {
        estado.lastError = "Token begin fail";
        return false;
    }

    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");

    String body = "{\"username\": \"admin\", \"password\": \"admin\"}";

    int code = http.POST(body);

    if (code != 200) {
        estado.lastError = "Token HTTP " + String(code);
        http.end();
        return false;
    }

    String resp = http.getString();
    DynamicJsonDocument doc(1024);

    if (deserializeJson(doc, resp)) {
        estado.lastError = "Parse token";
        http.end();
        return false;
    }

    if (doc["access"].isNull()) {
        estado.lastError = "Token sin access";
        http.end();
        return false;
    }

    jwtToken = doc["access"].as<String>();
    tokenObtainedAt = millis();

    estado.lastError = "";
    http.end();
    return true;
}
