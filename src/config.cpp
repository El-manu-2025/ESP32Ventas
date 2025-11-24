#include <Arduino.h>
#include "config.h"
#include "api_utils.h"

// URLs reales
const String API_BASE = "https://evaluacion1controldeventa-production.up.railway.app/api/";
const String TOKEN_ENDPOINT = "token/";
const String VENTAS_LATEST_ENDPOINT = "ventas/?ordering=-fecha&limit=1";

// Variables globales del estado
Estado estado;
long lastSaleId = -1;



