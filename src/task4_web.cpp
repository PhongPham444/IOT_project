// src/task4_web.cpp
#include "task4_web.h"
#include "task2_neopixel.h"   // neo_safe_set(SystemContext*, uint32_t)
#include "config.h"
#include "system_context.h"

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>

// DNS server for captive portal
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

// ---------------- HTML root with professional dashboard ----------------
static void handle_root(SystemContext* ctx) {
  String html = R"rawliteral(
    <!doctype html><html><head><meta charset="utf-8"><title>IoT Dashboard - YoloUNO</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
      * { box-sizing: border-box; margin: 0; padding: 0; }
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        min-height: 100vh;
        padding: 20px;
        display: flex;
        justify-content: center;
        align-items: center;
      }
      .dashboard {
        max-width: 1400px;
        width: 100%;
        background: rgba(255,255,255,0.98);
        border-radius: 20px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        padding: 30px;
      }
      .header {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 25px;
        padding-bottom: 20px;
        border-bottom: 2px solid #e0e0e0;
      }
      .header h1 {
        font-size: 28px;
        color: #2c3e50;
        font-weight: 700;
      }
      .network-badge {
        background: linear-gradient(135deg, #667eea, #764ba2);
        color: white;
        padding: 8px 16px;
        border-radius: 20px;
        font-size: 13px;
        font-weight: 600;
      }
      .grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
        gap: 20px;
        margin-bottom: 25px;
      }
      .card {
        background: white;
        border-radius: 15px;
        padding: 20px;
        box-shadow: 0 4px 15px rgba(0,0,0,0.08);
        transition: transform 0.2s, box-shadow 0.2s;
      }
      .card:hover {
        transform: translateY(-5px);
        box-shadow: 0 8px 25px rgba(0,0,0,0.15);
      }
      .card-title {
        font-size: 16px;
        color: #7f8c8d;
        margin-bottom: 15px;
        text-transform: uppercase;
        letter-spacing: 1px;
        font-weight: 600;
      }
      .sensor-value {
        font-size: 42px;
        font-weight: 700;
        color: #2c3e50;
        margin-bottom: 10px;
      }
      .sensor-label {
        font-size: 14px;
        color: #95a5a6;
      }
      .status-badge {
        display: inline-block;
        padding: 6px 14px;
        border-radius: 20px;
        font-size: 12px;
        font-weight: 700;
        margin-top: 10px;
        text-transform: uppercase;
      }
      .gauge-container {
        margin-top: 15px;
      }
      .gauge-bar {
        width: 100%;
        height: 16px;
        background: #ecf0f1;
        border-radius: 10px;
        overflow: hidden;
        position: relative;
      }
      .gauge-fill {
        height: 100%;
        background: linear-gradient(90deg, #667eea, #764ba2);
        border-radius: 10px;
        transition: width 0.5s ease;
        position: relative;
      }
      .gauge-fill.ai {
        background: linear-gradient(90deg, #f093fb, #f5576c);
      }
      .gauge-fill.light {
        background: linear-gradient(90deg, #ffd89b, #19547b);
      }
      .gauge-fill.moisture {
        background: linear-gradient(90deg, #a8edea, #fed6e3);
      }
      .gauge-label {
        display: flex;
        justify-content: space-between;
        margin-top: 8px;
        font-size: 13px;
        color: #7f8c8d;
      }
      .control-section {
        background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
        border-radius: 15px;
        padding: 25px;
        margin-bottom: 20px;
      }
      .control-title {
        font-size: 18px;
        color: #2c3e50;
        margin-bottom: 20px;
        font-weight: 700;
      }
      .button-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
        gap: 12px;
      }
      .btn {
        padding: 14px;
        border: none;
        border-radius: 10px;
        font-size: 14px;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.3s;
        color: white;
        text-transform: uppercase;
        letter-spacing: 0.5px;
      }
      .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }
      .btn:active { transform: translateY(0); }
      .btn.red { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }
      .btn.blue { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
      .btn.green { background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); }
      .btn.off { background: linear-gradient(135deg, #868f96 0%, #596164 100%); }
      .btn.relay { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); }
      .btn.blink { background: linear-gradient(135deg, #30cfd0 0%, #330867 100%); }
      .color-picker-section {
        margin-top: 20px;
        padding: 20px;
        background: white;
        border-radius: 12px;
        display: flex;
        align-items: center;
        gap: 15px;
      }
      input[type="color"] {
        width: 60px;
        height: 60px;
        border: 3px solid #e0e0e0;
        border-radius: 10px;
        cursor: pointer;
      }
      .footer {
        text-align: center;
        margin-top: 20px;
        padding-top: 15px;
        border-top: 1px solid #e0e0e0;
        color: #7f8c8d;
        font-size: 13px;
      }
      .footer a { color: #667eea; text-decoration: none; font-weight: 600; }
      .footer a:hover { text-decoration: underline; }
    </style>
    <script>
      function fetchText(path, opts){ return fetch(path, opts).then(r=>r.text()); }
      function toggleDevice(device){ fetchText('/'+device).then(t=>console.log(device,t)).catch(e=>console.error(e)); }
      function setColorRGB(r,g,b){
        fetchText('/setColor?r='+r+'&g='+g+'&b='+b).then(t=>console.log('setColor',t)).catch(e=>console.error(e));
      }
      function setColorFromPicker(){
        const hex = document.getElementById('colorpicker').value;
        const r = parseInt(hex.slice(1,3),16);
        const g = parseInt(hex.slice(3,5),16);
        const b = parseInt(hex.slice(5,7),16);
        setColorRGB(r,g,b);
      }
      function updateStatus(){
        fetch('/update').then(r=>r.json()).then(d=>{
          // Temperature & Humidity
          document.getElementById('temperature').innerText = (d.temperature!==undefined? d.temperature.toFixed(1): '--');
          document.getElementById('humidity').innerText = (d.humidity!==undefined? d.humidity.toFixed(1): '--');
          
          // Light & Moisture
          document.getElementById('light').innerText = (d.light!==undefined? (d.light*100).toFixed(1): '--');
          document.getElementById('moisture').innerText = (d.moisture!==undefined? (d.moisture*100).toFixed(1): '--');
          
          // AI Output
          const aiVal = d.aiOutput !== undefined ? d.aiOutput : 0;
          document.getElementById('aiOutput').innerText = aiVal.toFixed(3);
          document.getElementById('aiPrediction').innerText = aiVal > 0.4 ? 'HEATING ON' : 'HEATING OFF';
          
          // Update gauges
          document.getElementById('lightGauge').style.width = ((d.light || 0) * 100) + '%';
          document.getElementById('moistureGauge').style.width = ((d.moisture || 0) * 100) + '%';
          document.getElementById('aiGauge').style.width = (aiVal * 100) + '%';
          
          // Temperature state badge
          if (d.tempState) {
            const badge = document.getElementById('tempState');
            badge.innerText = d.tempState;
            badge.style.background = d.tempColor || '#4CAF50';
          }
          
          // Humidity state badge
          if (d.humState) {
            const badge = document.getElementById('humState');
            badge.innerText = d.humState;
            badge.style.background = d.humColor || '#4CAF50';
          }

          // Control states
          document.getElementById('relayState').innerText = d.relay ? 'ON' : 'OFF';
          document.getElementById('blinkState').innerText = d.blink ? 'ON' : 'OFF';
          
          // Network info
          if (d.network) document.getElementById('netInfo').innerText = d.network;
        }).catch(e=>console.error('update failed',e));
      }
      setInterval(updateStatus, 1000);
      window.onload = updateStatus;
    </script>
    </head>
    <body>
      <div class="dashboard">
        <div class="header">
          <h1>🌱 IoT Smart Agriculture Dashboard</h1>
          <div class="network-badge" id="netInfo">--</div>
        </div>

        <!-- Sensor Data Grid -->
        <div class="grid">
          <div class="card">
            <div class="card-title">🌡️ Temperature</div>
            <div class="sensor-value"><span id="temperature">--</span>°C</div>
            <div class="status-badge" id="tempState" style="background:#4CAF50;color:white">---</div>
          </div>

          <div class="card">
            <div class="card-title">💧 Humidity</div>
            <div class="sensor-value"><span id="humidity">--</span>%</div>
            <div class="status-badge" id="humState" style="background:#4CAF50;color:white">---</div>
          </div>

          <div class="card">
            <div class="card-title">☀️ Light Intensity</div>
            <div class="sensor-value"><span id="light">--</span>%</div>
            <div class="gauge-container">
              <div class="gauge-bar">
                <div class="gauge-fill light" id="lightGauge" style="width:0%"></div>
              </div>
              <div class="gauge-label">
                <span>Dark</span>
                <span>Bright</span>
              </div>
            </div>
          </div>

          <div class="card">
            <div class="card-title">💦 Soil Moisture</div>
            <div class="sensor-value"><span id="moisture">--</span>%</div>
            <div class="gauge-container">
              <div class="gauge-bar">
                <div class="gauge-fill moisture" id="moistureGauge" style="width:0%"></div>
              </div>
              <div class="gauge-label">
                <span>Dry</span>
                <span>Wet</span>
              </div>
            </div>
          </div>
        </div>

        <!-- AI Prediction Card -->
        <div class="card" style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white;">
          <div class="card-title" style="color: rgba(255,255,255,0.9)">🤖 AI Heating Control</div>
          <div class="sensor-value" style="color: white">
            <span id="aiOutput">0.000</span>
            <span style="font-size:20px; margin-left:15px; opacity:0.9" id="aiPrediction">--</span>
          </div>
          <div class="gauge-container">
            <div class="gauge-bar" style="background:rgba(255,255,255,0.3)">
              <div class="gauge-fill ai" id="aiGauge" style="width:0%; background:rgba(255,255,255,0.9)"></div>
            </div>
            <div class="gauge-label" style="color:rgba(255,255,255,0.9)">
              <span>Heating Off</span>
              <span>Heating On</span>
            </div>
          </div>
          <div class="sensor-label" style="color:rgba(255,255,255,0.8); margin-top:10px">
            Model: TensorFlow Lite | Threshold: >0.4 | Inputs: Temp, Humidity, Moisture, Light
          </div>
        </div>

        <!-- Control Section -->
        <div class="control-section">
          <div class="control-title">🎮 Device Controls</div>
          <div class="button-grid">
            <button class="btn red" onclick="toggleDevice('redLED')">Red LED</button>
            <button class="btn blue" onclick="toggleDevice('blueLED')">Blue LED</button>
            <button class="btn green" onclick="toggleDevice('greenLED')">Green LED</button>
            <button class="btn relay" onclick="toggleDevice('relay')">Relay: <span id="relayState">--</span></button>
            <button class="btn blink" onclick="toggleDevice('blink')">Blink: <span id="blinkState">--</span></button>
            <button class="btn off" onclick="toggleDevice('off')">All Off</button>
          </div>

          <div class="color-picker-section">
            <input type="color" id="colorpicker" value="#ff0000" onchange="setColorFromPicker()">
            <div>
              <div style="font-weight:600; margin-bottom:5px">Custom NeoPixel Color</div>
              <div style="font-size:13px; color:#7f8c8d">Pick any color to override automatic mode</div>
            </div>
          </div>
        </div>

        <div class="footer">
          <div>Configure WiFi: <a href="/wifi">/wifi</a> | Firmware Update: <a href="/ota">/ota</a></div>
          <div style="margin-top:8px">YoloUNO Smart Agriculture System | Powered by ESP32 + TensorFlow Lite</div>
        </div>
      </div>
    </body>
    </html>
  )rawliteral";

  ctx->server->send(200, "text/html", html);
}

// ---------------- /update JSON with sensor and AI data ----------------
static void handle_update(SystemContext* ctx) {
  float t = 0.0f, h = 0.0f;
  float light = 0.0f, moisture = 0.0f;
  float aiOutput = 0.0f;
  
  if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    t = ctx->latestTemp;
    h = ctx->latestHumid;
    light = ctx->latestLight;
    moisture = ctx->latestMoisture;
    aiOutput = ctx->latestAIOutput;
    xSemaphoreGive(ctx->sensorMutex);
  }

  // compute temp state and color
  String tempState = "NOR";
  String tempColor = "#4CAF50"; // green
  if (t >= ctx->critTemp) { tempState = "CRI"; tempColor = "#F44336"; }
  else if (t >= ctx->warnTemp) { tempState = "WAR"; tempColor = "#FFC107"; }

  // compute humidity state and color (using simplified thresholds)
  const float WARN_HUM = 60.0f;
  const float CRIT_HUM = 80.0f;
  String humState = "NOR";
  String humColor = "#4CAF50"; // green (NOR)
  if (h >= CRIT_HUM) { humState = "CRI"; humColor = "#F44336"; }    // red
  else if (h >= WARN_HUM) { humState = "WAR"; humColor = "#FFC107"; } // yellow


  String net = "";
  if (WiFi.status() == WL_CONNECTED) net = "STA:" + WiFi.SSID() + " IP:" + WiFi.localIP().toString();
  else net = "AP IP:" + WiFi.softAPIP().toString();

  // Build JSON with all sensor data
  String j = "{";
  j += "\"temperature\":" + String(t,2) + ",";
  j += "\"humidity\":" + String(h,2) + ",";
  j += "\"light\":" + String(light,4) + ",";
  j += "\"moisture\":" + String(moisture,4) + ",";
  j += "\"aiOutput\":" + String(aiOutput,4) + ",";
  j += "\"relay\":" + String((int)ctx->overrideRelay) + ",";
  j += "\"blink\":" + String((int)ctx->overrideLed) + ",";
  j += "\"tempState\":\"" + tempState + "\",";
  j += "\"tempColor\":\"" + tempColor + "\",";
  j += "\"humState\":\"" + humState + "\",";
  j += "\"humColor\":\"" + humColor + "\",";
  j += "\"network\":\"" + net + "\"";
  j += "}";
  ctx->server->send(200, "application/json", j);
}

// ---------------- WiFi config handlers (GET/POST) - save to Preferences ----------------
static void handle_wifi_get(SystemContext* ctx) {
  String html = "<html><body>"
    "<h3>Enter WiFi credentials (STA)</h3>"
    "<form method='POST' action='/wifi'>"
    "SSID: <input name='ssid'><br>"
    "PASS: <input name='pass' type='password'><br>"
    "<input type='submit' value='Connect & Save'>"
    "</form>"
    "<p>Current AP: " + String(AP_SSID) + "</p>"
    "</body></html>";
  ctx->server->send(200, "text/html", html);
}

static void handle_wifi_post(SystemContext* ctx) {
  if (!ctx->server->hasArg("ssid")) {
    ctx->server->send(400, "text/plain", "Missing ssid");
    return;
  }
  String ssid = ctx->server->arg("ssid");
  String pass = ctx->server->arg("pass");

  // persist using Preferences (NVS)
  Preferences prefs;
  if (prefs.begin("wifi", false)) {
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    Serial.printf("[WIFI] Saved credentials to NVS: %s\n", ssid.c_str());
  } else {
    Serial.println("[WIFI] Preferences begin failed - cannot save credentials");
  }

  // store in context so other tasks see them
  ctx->wifiSsid = ssid;
  ctx->wifiPass = pass;

  ctx->server->send(200, "text/html", "<html><body>Saved credentials. Attempting to connect...<p>Return to <a href='/'>main</a></p></body></html>");
  Serial.printf("[WIFI] Received STA credentials: SSID='%s'\n", ssid.c_str());

  // attempt quick connect (short wait)
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; ++i) {
    vTaskDelay(pdMS_TO_TICKS(300));
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] STA connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WIFI] STA connect not established (will retry in background).");
  }
}

// ---------------- Neo / color handlers ----------------
static void handle_red(SystemContext* ctx) {
  ctx->neo_r = 255; ctx->neo_g = 0; ctx->neo_b = 0;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200, "text/plain", "Red LED set");
}
static void handle_blue(SystemContext* ctx) {
  ctx->neo_r = 0; ctx->neo_g = 0; ctx->neo_b = 255;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200, "text/plain", "Blue LED set");
}
static void handle_green(SystemContext* ctx) {
  ctx->neo_r = 0; ctx->neo_g = 255; ctx->neo_b = 0;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200, "text/plain", "Green LED set");
}
static void handle_off(SystemContext* ctx) {
  ctx->overrideNeo = false;
  ControlMsg m = {0, false, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200, "text/plain", "LEDs turned off");
}

// set arbitrary RGB via query /setColor?r=RR&g=GG&b=BB
static void handle_setColor(SystemContext* ctx) {
  if (!ctx->server->hasArg("r") || !ctx->server->hasArg("g") || !ctx->server->hasArg("b")) {
    ctx->server->send(400, "text/plain", "missing r/g/b");
    return;
  }
  int r = ctx->server->arg("r").toInt();
  int g = ctx->server->arg("g").toInt();
  int b = ctx->server->arg("b").toInt();
  ctx->neo_r = (uint8_t)constrain(r,0,255);
  ctx->neo_g = (uint8_t)constrain(g,0,255);
  ctx->neo_b = (uint8_t)constrain(b,0,255);
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200, "text/plain", "Color queued");
}

// ---------------- Relay / Blink handlers ----------------
static void handle_relay(SystemContext* ctx) {
  ControlMsg m = {3, !ctx->overrideRelay, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200, "text/plain", "Relay toggle queued");
}
static void handle_blink(SystemContext* ctx) {
  ControlMsg m = {1, !ctx->overrideLed, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200, "text/plain", "Blink toggle queued");
}

// ---------------- OTA handlers (/ota) ----------------
// GET: simple upload form
static void handle_ota_get(SystemContext* ctx) {
  String html = "<html><body><h3>Upload firmware (.bin)</h3>"
    "<form method='POST' action='/ota' enctype='multipart/form-data'>"
    "<input type='file' name='firmware'>"
    "<input type='submit' value='Upload'>"
    "</form></body></html>";
  ctx->server->send(200, "text/html", html);
}

// POST: handle upload stream (uses WebServer upload API)
static void handle_ota_upload(SystemContext* ctx) {
  HTTPUpload& upload = ctx->server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Update Start: %s\n", upload.filename.c_str());
    // start with unknown size allowed
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // write chunk
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Update Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    Serial.println("[OTA] Update aborted");
  }
}

// ---------------- Captive Portal Handler ----------------
static void handle_captive(SystemContext* ctx) {
  // Redirect all requests to root
  String host = ctx->server->hostHeader();
  if (host.indexOf(WiFi.softAPIP().toString()) < 0) {
    // If not accessing by IP, redirect
    ctx->server->sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    ctx->server->send(302, "text/plain", "");
  } else {
    handle_root(ctx);
  }
}

// ---------------- Logo handler ----------------
static void handle_logo(SystemContext* ctx) {
  // Simple 1x1 transparent PNG (placeholder - replace with actual logo data)
  static const uint8_t logo_png[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
    0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
    0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
  };
  ctx->server->send_P(200, "image/png", (const char*)logo_png, sizeof(logo_png));
}

// ---------------- register routes ----------------
void web_setup_routes(SystemContext* ctx) {
  ctx->server->on("/", [ctx]() { handle_root(ctx); });
  ctx->server->on("/update", [ctx]() { handle_update(ctx); });
  ctx->server->on("/logo.png", [ctx]() { handle_logo(ctx); });

  ctx->server->on("/wifi", HTTP_GET, [ctx]() { handle_wifi_get(ctx); });
  ctx->server->on("/wifi", HTTP_POST, [ctx]() { handle_wifi_post(ctx); });

  ctx->server->on("/redLED", [ctx]() { handle_red(ctx); });
  ctx->server->on("/blueLED", [ctx]() { handle_blue(ctx); });
  ctx->server->on("/greenLED", [ctx]() { handle_green(ctx); });
  ctx->server->on("/off", [ctx]() { handle_off(ctx); });

  ctx->server->on("/setColor", [ctx]() { handle_setColor(ctx); });

  ctx->server->on("/relay", [ctx]() { handle_relay(ctx); });
  ctx->server->on("/blink", [ctx]() { handle_blink(ctx); });

  // OTA: GET form, and POST upload handler + upload processor
  ctx->server->on("/ota", HTTP_GET, [ctx]() { handle_ota_get(ctx); });
  ctx->server->on("/ota", HTTP_POST, [ctx]() {
    // after upload finished, send final response
    ctx->server->sendHeader("Connection","close");
    ctx->server->send(200, "text/plain", "OTA upload finished. If successful, device will reboot.");
  }, [ctx]() { // upload handler called repeatedly
    handle_ota_upload(ctx);
  });

  // Captive portal catch-all - must be last
  ctx->server->onNotFound([ctx]() { handle_captive(ctx); });

  // Start DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  ctx->server->begin();
  Serial.println("[Captive Portal] DNS and Web server started");
}

// ---------------- TaskControlConsumer (unchanged except fan removed) ----------------
void TaskControlConsumer(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);
  ControlMsg m;
  for (;;) {
    if (ctx->controlSem) {
      if (xSemaphoreTake(ctx->controlSem, portMAX_DELAY) != pdTRUE) continue;
    } else vTaskDelay(pdMS_TO_TICKS(100));

    while (xQueueReceive(ctx->controlQueue, &m, 0) == pdTRUE) {
      switch(m.device) {
        case 0:
          if (ctx->pixels) neo_safe_set(ctx, ctx->pixels->Color(0,0,0));
          ctx->overrideNeo = false;
          ctx->overrideLed = false;
          ctx->overrideRelay = false;
          Serial.println("[Control] All Off applied");
          break;
        case 1: {
          ctx->overrideLed = m.on;
          Serial.printf("[Control] overrideLed = %d\n", (int)ctx->overrideLed);

          if (xSemaphoreTake(ctx->ledNeoSem, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (ctx->pixels_task1) {
              if (ctx->overrideLed)
                ctx->pixels_task1->setPixelColor(0, ctx->pixels_task1->Color(0, 0, 0));
              else
                ctx->pixels_task1->setPixelColor(0, ctx->pixels_task1->Color(0, 0, 0));       // off
              ctx->pixels_task1->show();
            }
            xSemaphoreGive(ctx->ledNeoSem);
          }
          break;
        }
        case 3:
          ctx->overrideRelay = m.on;
          digitalWrite(PIN_RELAY, ctx->overrideRelay ? HIGH : LOW);
          Serial.printf("[Control] Relay set to %d\n", (int)ctx->overrideRelay);
          break;
        default:
          Serial.printf("[Control] Unknown device %u\n", m.device);
          break;
      }
    }
  }
}

// ---------------- TaskServer ----------------
void TaskServer(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);
  const TickType_t tick = pdMS_TO_TICKS(10);
  for (;;) {
    // Process DNS requests for captive portal
    dnsServer.processNextRequest();
    // Handle web requests
    if (ctx->server) ctx->server->handleClient();
    vTaskDelay(tick);
  }
}
