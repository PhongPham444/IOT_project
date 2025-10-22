#include "task4_web.h"
#include "task2_neopixel.h"   // neo_safe_set
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Arduino.h>

/* HTML UI: two-column layout, color buttons, fan slider, relay & blink toggle */
static void handle_root(SystemContext* ctx) {
  String html = R"rawliteral(
    <html>
    <head>
        <title>Control Panel</title>
        <meta charset="UTF-8">
        <style>
            body {
                display: flex;
                justify-content: center;
                align-items: center;
                height: 100vh;
                margin: 0;
                font-family: Arial, sans-serif;
                background-color: #f2f2f2;
            }
            .container {
                text-align: center;
                background-color: #ffffff;
                padding: 20px;
                border-radius: 10px;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);
                width: 850px;
            }
            .content {
                display: flex;
                flex-direction: row;
                justify-content: space-between;
                margin-top: 20px;
                gap: 20px;
            }
            .column {
                width: 48%;
                padding: 10px;
            }
            h1 {
                color: #333;
                margin-bottom: 0;
            }
            .button {
                display: block;
                width: 160px;
                padding: 12px;
                margin: 10px auto;
                font-size: 16px;
                color: #ffffff;
                border: none;
                border-radius: 5px;
                cursor: pointer;
                transition: background-color 0.3s;
            }
            .button.red { background-color: #F44336; }
            .button.blue { background-color: #2196F3; }
            .button.green { background-color: #4CAF50; }
            .button.off { background-color: #9E9E9E; color:#222; }
            .button.relay { background-color: #FF9800; }
            .button.blink { background-color: #607D8B; }
            .slider-container {
                margin-top: 20px;
                display: flex;
                flex-direction: column;
                align-items: center;
            }
            .slider {
                width: 100%;
                max-width: 380px;
            }
            .data-container {
                display: flex;
                flex-direction: column;
                align-items: flex-start;
                margin-top: 10px;
                padding: 10px;
                background-color: #f9f9f9;
                border: 1px solid #ddd;
                border-radius: 5px;
                width: 100%;
            }
            .data-item {
                display: flex;
                justify-content: space-between;
                width: 100%;
                font-size: 18px;
                padding: 6px 0;
            }
            .label { text-align: left; }
            .value { text-align: right; }
        </style>
        <script>
            function toggleDevice(device) {
                fetch('/' + device)
                    .then(response => response.text())
                    .then(data => console.log(device + ' toggled', data))
                    .catch(error => console.error('Error:', error));
            }

            function setFanSpeed(value) {
                fetch('/setFanSpeed?speed=' + value)
                    .then(response => response.text())
                    .then(data => {
                        console.log('Fan speed set to', value);
                        document.getElementById("fanSpeedValue").textContent = value;
                    })
                    .catch(error => console.error('Error:', error));
            }

            function updateSensorData() {
                fetch('/update')
                    .then(response => response.json())
                    .then(data => {
                        document.getElementById("temperature").textContent = data.temperature.toFixed(1) + " °C";
                        document.getElementById("humidity").textContent = data.humidity.toFixed(1) + " %";
                        document.getElementById("fanSpeed").value = data.fanSpeed;
                        document.getElementById("fanSpeedValue").textContent = data.fanSpeed;
                        document.getElementById("relayState").textContent = data.relay ? "ON" : "OFF";
                        document.getElementById("blinkState").textContent = data.blink ? "ON" : "OFF";
                    })
                    .catch(error => console.error('Error:', error));
            }

            setInterval(updateSensorData, 1000);
        </script>
    </head>
    <body>
        <div class="container">
            <h1>Control Panel</h1>
            <div class="content">
                <div class="column">
                    <div class="data-container">
                        <div class="data-item"><span class="label">Temperature:</span> <span id="temperature" class="value">-- °C</span></div>
                        <div class="data-item"><span class="label">Humidity:</span> <span id="humidity" class="value">-- %</span></div>
                        <div class="data-item"><span class="label">Fan Speed:</span> <span id="fanSpeedValue" class="value">--</span></div>
                        <div class="data-item"><span class="label">Relay:</span> <span id="relayState" class="value">--</span></div>
                        <div class="data-item"><span class="label">Blink LED:</span> <span id="blinkState" class="value">--</span></div>
                    </div>

                    <div class="slider-container">
                        <label for="fanSpeed">Fan Speed:</label>
                        <input type="range" id="fanSpeed" class="slider" min="0" max="255" value="128" onchange="setFanSpeed(this.value)">
                    </div>
                </div>

                <div class="column">
                    <button class="button red" onclick="toggleDevice('redLED')">Red</button>
                    <button class="button blue" onclick="toggleDevice('blueLED')">Blue</button>
                    <button class="button green" onclick="toggleDevice('greenLED')">Green</button>
                    <button class="button off" onclick="toggleDevice('off')">Turn Off LEDs</button>
                    <button class="button relay" onclick="toggleDevice('relay')">Toggle Relay</button>
                    <button class="button blink" onclick="toggleDevice('blink')">Toggle Blink LED</button>
                </div>
            </div>
        </div>
    </body>
    </html>
  )rawliteral";

  ctx->server->send(200,"text/html",html);
}

/* ---- /update route: return JSON (temperature, humidity, fanSpeed, relay, blink) ---- */
static void handle_update(SystemContext* ctx) {
  float t = 0.0f, h = 0.0f;
  if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    t = ctx->latestTemp;
    h = ctx->latestHumid;
    xSemaphoreGive(ctx->sensorMutex);
  }
  String j = "{";
  j += "\"temperature\":" + String(t,2) + ",";
  j += "\"humidity\":" + String(h,2) + ",";
  j += "\"fanSpeed\":" + String((int)ctx->fanSpeed) + ",";
  j += "\"relay\":" + String((int)ctx->overrideRelay) + ",";
  j += "\"blink\":" + String((int)ctx->overrideLed);
  j += "}";
  ctx->server->send(200,"application/json",j);
}

/* ---- color handlers: set neo color and give neoSem (manual override) ---- */
static void handle_red(SystemContext* ctx) {
  ctx->neo_r = 255; ctx->neo_g = 0; ctx->neo_b = 0;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200,"text/plain","Red LED set");
}
static void handle_blue(SystemContext* ctx) {
  ctx->neo_r = 0; ctx->neo_g = 0; ctx->neo_b = 255;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200,"text/plain","Blue LED set");
}
static void handle_green(SystemContext* ctx) {
  ctx->neo_r = 0; ctx->neo_g = 255; ctx->neo_b = 0;
  ctx->overrideNeo = true;
  if (ctx->neoSem) xSemaphoreGive(ctx->neoSem);
  ctx->server->send(200,"text/plain","Green LED set");
}
static void handle_off(SystemContext* ctx) {
  ctx->overrideNeo = false;
  // enqueue an "all off" control msg so consumer will clear LEDs
  ControlMsg m = {0, false, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200,"text/plain","LEDs turned off");
}

/* ---- relay toggle handler: enqueue control message and signal semaphore ---- */
static void handle_relay(SystemContext* ctx) {
  // toggle flag (consumer will apply actual pin)
  ControlMsg m = {3, !ctx->overrideRelay, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200,"text/plain","Relay toggle queued");
}

/* ---- blink toggle handler: toggle overrideLed via control queue ---- */
static void handle_blink(SystemContext* ctx) {
  ControlMsg m = {1, !ctx->overrideLed, 0};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200,"text/plain","Blink toggle queued");
}

/* ---- fan speed handler: parse speed param -> enqueue control msg with param ---- */
static void handle_setFanSpeed(SystemContext* ctx) {
  if (!ctx->server->hasArg("speed")) {
    ctx->server->send(400,"text/plain","missing speed");
    return;
  }
  int speed = ctx->server->arg("speed").toInt();
  if (speed < 0) speed = 0; if (speed > 255) speed = 255;
  ControlMsg m = {5, speed>0 ? true : false, speed};
  xQueueSend(ctx->controlQueue, &m, 0);
  if (ctx->controlSem) xSemaphoreGive(ctx->controlSem);
  ctx->server->send(200,"text/plain","Fan speed queued");
}

/* ---- setup routes ---- */
void web_setup_routes(SystemContext* ctx) {
  ctx->server->on("/", [ctx]() { handle_root(ctx); });
  ctx->server->on("/update", [ctx]() { handle_update(ctx); });

  ctx->server->on("/redLED", [ctx]() { handle_red(ctx); });
  ctx->server->on("/blueLED", [ctx]() { handle_blue(ctx); });
  ctx->server->on("/greenLED", [ctx]() { handle_green(ctx); });
  ctx->server->on("/off", [ctx]() { handle_off(ctx); });

  ctx->server->on("/relay", [ctx]() { handle_relay(ctx); });
  ctx->server->on("/blink", [ctx]() { handle_blink(ctx); });
  ctx->server->on("/setFanSpeed", HTTP_GET, [ctx]() { handle_setFanSpeed(ctx); });

  ctx->server->begin();
}

/* ---------------- TaskControlConsumer ----------------
   Waits on ctx->controlSem, then consumes controlQueue (one or multiple entries).
   Handles:
     0 -> all off: clears Neo & reset overrides
     1 -> LED blink override (ctx->overrideLed)
     3 -> Relay toggle
     5 -> Fan speed (param)
   For fan PWM we use ledc (ESP32): ledcWrite(channel, speed).
*/
void TaskControlConsumer(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  ControlMsg m;
  for (;;) {
    // wait until a control command is signalled
    if (ctx->controlSem) {
      if (xSemaphoreTake(ctx->controlSem, portMAX_DELAY) != pdTRUE) continue;
    } else {
      // fallback: poll queue
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    // drain queue
    while (xQueueReceive(ctx->controlQueue, &m, 0) == pdTRUE) {
      switch (m.device) {
        case 0: // all off
          if (ctx->pixels) neo_safe_set(ctx, ctx->pixels->Color(0,0,0));
          ctx->overrideNeo = false;
          ctx->overrideLed = false;
          ctx->overrideRelay = false;
          ctx->controlFan = false;
          ctx->fanSpeed = 0;
          // set PWM to 0
          analogWrite(FAN_PIN, 0);
          Serial.println("[Control] All Off applied");
          break;

        case 1: // LED blink override
          ctx->overrideLed = m.on;
          digitalWrite(PIN_LED_BLINK, ctx->overrideLed ? HIGH : LOW);
          Serial.printf("[Control] overrideLed set to %d\n", (int)ctx->overrideLed);
          break;

        case 3: // Relay toggle
          ctx->overrideRelay = m.on;
          digitalWrite(PIN_RELAY, ctx->overrideRelay ? HIGH : LOW);
          Serial.printf("[Control] Relay set to %d\n", (int)ctx->overrideRelay);
          break;

        case 5: // Fan speed
          ctx->fanSpeed = (uint8_t)constrain(m.param, 0, 255);
          ctx->controlFan = ctx->fanSpeed > 0;
          // Use analogWrite like in your sample
          analogWrite(FAN_PIN, ctx->fanSpeed);
          Serial.printf("[Control] Fan speed set to %u\n", (unsigned)ctx->fanSpeed);
          break;

        default:
          Serial.printf("[Control] Unknown device %u\n", m.device);
          break;
      }
    }
  }
}

/* ---------------- TaskServer ----------------
   runs ctx->server->handleClient() periodically
*/
void TaskServer(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);
  const TickType_t tick = pdMS_TO_TICKS(10);
  for (;;) {
    if (ctx->server) ctx->server->handleClient();
    vTaskDelay(tick);
  }
}
