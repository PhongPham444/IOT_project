// src/task4_web.cpp
#include "task4_web.h"
#include "task2_neopixel.h"   // neo_safe_set(SystemContext*, uint32_t)
#include "config.h"
#include "system_context.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>

// ---------------- HTML root with color picker (no fan) ----------------
static void handle_root(SystemContext* ctx) {
  String html = R"rawliteral(
    <!doctype html><html><head><meta charset="utf-8"><title>YoloUNO Control Panel</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
      body{display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;font-family:Arial,Helvetica,sans-serif;background:#f2f2f2}
      .container{width:900px;background:#fff;border-radius:10px;padding:18px;box-shadow:0 8px 24px rgba(0,0,0,0.12)}
      h1{margin:0 0 8px 0;font-size:20px;color:#222}
      .content{display:flex;gap:20px;margin-top:12px}
      .col{flex:1;min-width:260px}
      .data{background:#fafafa;border:1px solid #eee;padding:10px;border-radius:6px}
      .row{display:flex;justify-content:space-between;padding:6px 0;font-size:16px}
      .value-box{display:inline-block;padding:6px 10px;border-radius:6px;color:#fff;font-weight:600;min-width:90px;text-align:center}
      .controls{display:flex;flex-direction:column;gap:10px;align-items:center}
      .btn{width:160px;padding:10px;border-radius:6px;border:none;color:#fff;cursor:pointer;font-weight:600}
      .btn.red{background:#e53935}.btn.blue{background:#1e88e5}.btn.green{background:#43a047}.btn.off{background:#9e9e9e;color:#222}
      .btn.relay{background:#fb8c00}.btn.blink{background:#455a64}
      .small{font-size:13px;color:#666;margin-top:6px}
      .colorRow{display:flex;gap:10px;align-items:center;margin-top:8px}
      input[type="color"]{width:56px;height:36px;border:0;padding:0;background:#fff;border-radius:6px}
      .legend { margin-top:8px; font-size:13px; color:#666 }
      .stat-label { margin-right: 8px; color:#333; }
    </style>
    <script>
      function fetchText(path, opts){ return fetch(path, opts).then(r=>r.text()); }
      function toggleDevice(device){ fetchText('/'+device).then(t=>console.log(device,t)).catch(e=>console.error(e)); }
      function setColorRGB(r,g,b){
        fetchText('/setColor?r='+r+'&g='+g+'&b='+b).then(t=>console.log('setColor',t)).catch(e=>console.error(e));
      }
      function setColorFromPicker(){
        const hex = document.getElementById('colorpicker').value; // "#RRGGBB"
        const r = parseInt(hex.slice(1,3),16);
        const g = parseInt(hex.slice(3,5),16);
        const b = parseInt(hex.slice(5,7),16);
        setColorRGB(r,g,b);
      }
      function updateStatus(){
        fetch('/update').then(r=>r.json()).then(d=>{
          // numerical values
          document.getElementById('temperature').innerText = (d.temperature!==undefined? d.temperature.toFixed(1): '--') + ' °C';
          document.getElementById('humidity').innerText = (d.humidity!==undefined? d.humidity.toFixed(1): '--') + ' %';
          // set background color based on provided color fields (hex)
          if (d.tempColor) {
            const el = document.getElementById('temperatureBox');
            el.style.backgroundColor = d.tempColor;
            // ensure text color contrast for dark/light backgrounds:
            el.style.color = (isLightColor(d.tempColor) ? '#222' : '#fff');
          }
          if (d.humColor) {
            const el2 = document.getElementById('humidityBox');
            el2.style.backgroundColor = d.humColor;
            el2.style.color = (isLightColor(d.humColor) ? '#222' : '#fff');
          }
          // states (text)
          if (d.tempState) document.getElementById('tempState').innerText = d.tempState;
          if (d.humState) document.getElementById('humState').innerText = d.humState;

          document.getElementById('relayState').innerText = d.relay ? 'ON' : 'OFF';
          document.getElementById('blinkState').innerText = d.blink ? 'ON' : 'OFF';
          if (d.network) document.getElementById('netInfo').innerText = d.network;
        }).catch(e=>console.error('update failed',e));
      }
      // simple luminance check to choose text color
      function isLightColor(hex){
        if (!hex || hex[0] !== '#') return false;
        const r = parseInt(hex.slice(1,3),16);
        const g = parseInt(hex.slice(3,5),16);
        const b = parseInt(hex.slice(5,7),16);
        // relative luminance
        const lum = 0.2126*r + 0.7152*g + 0.0722*b;
        return lum > 150;
      }
      setInterval(updateStatus,1000);
      window.onload = updateStatus;
    </script>
    </head>
    <body>
      <div class="container">
        <h1>YoloUNO Control Panel</h1>
        <div class="content">
          <div class="col">
            <div class="data">
              <div class="row"><span class="stat-label">Temperature:</span>
                <span id="temperatureBox" class="value-box" style="background:#666"><span id="temperature">-- °C</span><br><small id="tempState" style="font-size:11px;opacity:0.9">---</small></span>
              </div>
              <div class="row"><span class="stat-label">Humidity:</span>
                <span id="humidityBox" class="value-box" style="background:#666"><span id="humidity">-- %</span><br><small id="humState" style="font-size:11px;opacity:0.9">---</small></span>
              </div>
              <div class="row"><span>Relay:</span><span id="relayState">--</span></div>
              <div class="row"><span>Blink LED:</span><span id="blinkState">--</span></div>
              <div class="small">Network: <span id="netInfo">--</span></div>
            </div>

            <div style="margin-top:12px;text-align:center">
              <div class="small">Color picker (manual override NeoPixel)</div>
              <div class="colorRow">
                <input type="color" id="colorpicker" value="#ff0000" onchange="setColorFromPicker()">
                <button class="btn off" onclick="toggleDevice('off')">Turn Off LEDs</button>
              </div>
              <div class="legend">Temp colors: <span style="display:inline-block;width:14px;height:10px;background:#4CAF50;margin:0 6px;border-radius:2px"></span>NOR <span style="display:inline-block;width:14px;height:10px;background:#FFC107;margin:0 6px;border-radius:2px"></span>WAR <span style="display:inline-block;width:14px;height:10px;background:#F44336;margin:0 6px;border-radius:2px"></span>CRI</div>
              <div class="legend">Hum colors: <span style="display:inline-block;width:14px;height:10px;background:#4CAF50;margin:0 6px;border-radius:2px"></span>NOR <span style="display:inline-block;width:14px;height:10px;background:#FFC107;margin:0 6px;border-radius:2px"></span>WAR <span style="display:inline-block;width:14px;height:10px;background:#F44336;margin:0 6px;border-radius:2px"></span>CRI</div>
              <div class="small">To configure WiFi: visit <a href="/wifi">/wifi</a></div>
              <div class="small"><a href="/ota">Firmware OTA</a> (use with care)</div>
            </div>
          </div>

          <div class="col controls">
            <button class="btn red" onclick="toggleDevice('redLED')">Red</button>
            <button class="btn blue" onclick="toggleDevice('blueLED')">Blue</button>
            <button class="btn green" onclick="toggleDevice('greenLED')">Green</button>
            <button class="btn relay" onclick="toggleDevice('relay')">Toggle Relay</button>
            <button class="btn blink" onclick="toggleDevice('blink')">Toggle Blink LED</button>
          </div>
        </div>
      </div>
    </body>
    </html>
  )rawliteral";

  ctx->server->send(200, "text/html", html);
}

// ---------------- /update JSON (no fan) ----------------
static void handle_update(SystemContext* ctx) {
  float t = 0.0f, h = 0.0f;
  if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    t = ctx->latestTemp;
    h = ctx->latestHumid;
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

  // Build JSON (manual string assembly to avoid heavy libs)
  String j = "{";
  j += "\"temperature\":" + String(t,2) + ",";
  j += "\"humidity\":" + String(h,2) + ",";
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

// ---------------- register routes ----------------
void web_setup_routes(SystemContext* ctx) {
  ctx->server->on("/", [ctx]() { handle_root(ctx); });
  ctx->server->on("/update", [ctx]() { handle_update(ctx); });

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

  ctx->server->begin();
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
    if (ctx->server) ctx->server->handleClient();
    vTaskDelay(tick);
  }
}
