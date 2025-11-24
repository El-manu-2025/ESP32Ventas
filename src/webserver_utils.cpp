#include <ArduinoJson.h>
#include "webserver_utils.h"
#include "api_utils.h"
#include <WebServer.h>

// Aquí se crea la instancia real del servidor
WebServer server(80);

// ---- PROTOTIPOS INTERNOS ----
String pageRootHTML();

// ---- CONFIG DEL SERVIDOR ----
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/status", handleStatusJSON);
    server.begin();
    Serial.println("Servidor web iniciado en puerto 80");
}

// ---- RUTA PRINCIPAL HTML ----
void handleRoot() {
    server.send(200, "text/html", pageRootHTML());
}

// ---- JSON PARA EL PANEL ----
void handleStatusJSON() {
    JsonDocument doc;
    doc["saleId"]            = estado.saleId;
    doc["saleRawJson"]       = estado.saleRawJson;
    doc["lastError"]         = estado.lastError;
    doc["lastSaleSuccess"]   = estado.lastSaleSuccess;
    doc["lastProductCodigo"] = estado.lastProductCodigo;
    doc["lastProductName"] = estado.lastProductName;
    doc["lastProductStock"]  = estado.lastProductStock;
    doc["lastProductId"]     = estado.lastProductId;

    doc["wifiSSID"] = WiFi.SSID();
    doc["wifiRSSI"] = WiFi.RSSI();


    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ---- HTML COMPLETO PARA EL CELULAR ----
String pageRootHTML() {
    return R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Panel ESP32 - Ventas</title>

<style>
  body{
    font-family: Arial, sans-serif;
    margin: 12px;
    background: #f4f4f4;
  }

  h2{
    text-align:center;
    margin-bottom: 15px;
  }

  /* -------- WIFI -------- */
  #wifiBox{
    display:flex;
    align-items:center;
    gap:10px;
    margin-bottom:15px;
    background:white;
    padding:10px;
    border-radius:10px;
    box-shadow:0 2px 6px rgba(0,0,0,0.15);
    opacity:0;
    transform: translateY(10px);
    transition: all .35s ease;
  }

  #wifiBox.show{
    opacity:1;
    transform: translateY(0);
  }

  #wifiSignal{
    font-size:26px;
    font-weight:bold;
  }

  #wifiInfo{
    font-size:13px;
    line-height:1.1;
  }

  .green{ color:#00c200; }
  .yellow{ color:#e3b400; }
  .red{ color:#d10000; }

  /* -------- TARJETAS -------- */
  .card{
    background: white;
    border-radius: 10px;
    padding: 12px;
    margin-bottom: 15px;
    box-shadow: 0 2px 6px rgba(0,0,0,0.15);
    opacity:0;
    transform: translateY(10px);
    transition: all .35s ease;
  }

  .card.show{
    opacity:1;
    transform: translateY(0);
  }

  .title{
    font-size: 16px;
    font-weight: bold;
    margin-bottom: 6px;
    display: flex;
    align-items: center;
    gap:6px;
  }

  .success{ color: #1a7f37; font-weight:bold; }
  .error{ color: #c62828; font-weight:bold; }
  .info{ color:#0277bd; font-weight:bold; }

  .tag{
    background:#eee;
    border-radius:6px;
    padding:3px 6px;
    font-size:13px;
    display:inline-block;
    margin-top:4px;
  }

  .spin {
    display:inline-block;
    width:14px;
    height:14px;
    border:3px solid #0277bd;
    border-top-color:transparent;
    border-radius:50%;
    animation: girar 0.8s linear infinite;
  }

  @keyframes girar {
    to { transform:rotate(360deg); }
  }
</style>
</head>

<body>

<h2>📊 Panel ESP32 - Ventas</h2>

<!-- INDICADOR WIFI -->
<div id="wifiBox">
  <span id="wifiSignal">🟥🟥🟥🟥</span>
  <div id="wifiInfo">
    <div id="wifiSSID">SSID: --</div>
    <div id="wifiRSSI">RSSI: -- dBm</div>
    <div id="wifiQuality">Calidad: --%</div>
  </div>
</div>

<div id="ventaCard" class="card"></div>
<div id="productoCard" class="card"></div>
<div id="estadoCard" class="card">
  <div class="title">📟 Estado</div>
  <div id="estadoTexto">Obteniendo datos…</div>
</div>


<script>

// ----------- WIFI ------------

function updateWifi(rssi, ssid){
  const icon = document.getElementById("wifiSignal");

  let quality = Math.min(100, Math.max(0, 2 * (rssi + 100))); 
  document.getElementById("wifiQuality").textContent = "Calidad: " + quality + "%";

  let bars = "";
  let colorClass = "";

  if(rssi >= -55){ 
    bars = "🟩🟩🟩🟩"; 
    colorClass = "green";
  }
  else if(rssi >= -65){ 
    bars = "🟩🟩🟩🟥"; 
    colorClass = "green";
  }
  else if(rssi >= -75){ 
    bars = "🟨🟨🟥🟥"; 
    colorClass = "yellow"; 
  }
  else{ 
    bars = "🟥🟥🟥🟥"; 
    colorClass = "red";
  }

  icon.textContent = bars;
  icon.className = colorClass;

  document.getElementById("wifiSSID").textContent = "SSID: " + ssid;
  document.getElementById("wifiRSSI").textContent = "RSSI: " + rssi + " dBm";
}

function notificarDesconexion(){
  const icon = document.getElementById("wifiSignal");
  icon.textContent = "🟥🟥🟥🟥";
  icon.className = "red";

  document.getElementById("estadoTexto").innerHTML =
    `<span class="error">Sin conexión al servidor</span>`;
}


// ----------- CARGA DE ESTADO ------------

async function load(){
  try{
    let r = await fetch('/status');
    let j = await r.json();

    updateWifi(j.wifiRSSI, j.wifiSSID);

    // ----- TARJETA VENTA -----
    let ventaHTML = `
      <div class="title">🧾 Última Venta</div>
      <div>ID de venta: <span class="tag">${j.saleId}</span></div>
      <div>Cantidad vendida: <span class="tag">${j.lastProductStock}</span></div>
    `;

    // ----- TARJETA PRODUCTO -----
    let productoHTML = `
      <div class="title">📦 Producto vendido</div>
      <div>Nombre: <span class="tag">${j.lastProductName || "-"}</span></div>
      <div>Código: <span class="tag">${j.lastProductCodigo || "-"}</span></div>
    `;

    // ----- TARJETA ESTADO -----
    let estadoHTML = `
      <div class="title">📟 Estado</div>
    `;

    if (j.saleId === 0) {
      estadoHTML += `<div class="info">Obteniendo datos <span class="spin"></span></div>`;
    }
    else if (j.lastSaleSuccess) {
      estadoHTML += `<div class="success">✔ Venta procesada correctamente</div>`;
    }
    else {
      let errorText = j.lastError || "Error desconocido";
      estadoHTML += `<div class="error">✘ ${errorText}</div>`;
    }

    // Anti-parpadeo
    const estadoCard = document.getElementById("estadoCard");
    if (estadoCard.dataset.last !== estadoHTML) {
        estadoCard.innerHTML = estadoHTML;
        estadoCard.dataset.last = estadoHTML;
    }

    document.getElementById("ventaCard").innerHTML = ventaHTML;
    document.getElementById("productoCard").innerHTML = productoHTML;

    setTimeout(()=>{
      document.getElementById("wifiBox").classList.add("show");
      document.getElementById("ventaCard").classList.add("show");
      document.getElementById("productoCard").classList.add("show");
      document.getElementById("estadoCard").classList.add("show");
    }, 50);

  } catch(e) {
    notificarDesconexion();
  }
}

setInterval(load, 4000);
load();

</script>

</body>
</html>
)rawliteral";
}
