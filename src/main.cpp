// src/main.cpp
#include <Arduino.h>
#include "system_context.h"
#include "config.h"

// task headers
#include "task1_blink.h"
#include "task2_neopixel.h"
#include "task3_dht_lcd.h"
#include "task4_web.h"
#include "task5_tinyml.h"
#include "light.h"
#include "task6_coreiot.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting refactored project...");

  // allocate context on heap (no globals)
  SystemContext* ctx = (SystemContext*) malloc(sizeof(SystemContext));
  if (!ctx) {
    Serial.println("Failed to allocate SystemContext");
    for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  memset(ctx, 0, sizeof(SystemContext));
  Preferences prefs;
  if (prefs.begin("wifi", true)) { // read-only namespace
    String saved_ssid = prefs.getString("ssid", "");
    String saved_pass = prefs.getString("pass", "");
    prefs.end();
    if (saved_ssid.length() > 0) {
      ctx->wifiSsid = saved_ssid;
      ctx->wifiPass = saved_pass;
      Serial.printf("[MAIN] Loaded saved WiFi SSID from NVS: %s\n", saved_ssid.c_str());
    }
  }
  // allocate hardware objects
  ctx->pixels_task1 = new Adafruit_NeoPixel(LED_COUNT, PIN_LED_BLINK, NEO_GRB + NEO_KHZ800);
  ctx->pixels = new Adafruit_NeoPixel(NEO_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
  ctx->dht = new DHT20();
  ctx->lcd = new LiquidCrystal_I2C(0x21, 16, 2);
  ctx->server = new WebServer(80);

  // network/mqtt pointers created in mqtt_init (called below)
  ctx->wifiClient = nullptr;
  ctx->mqttClient = nullptr;

  // default thresholds
  ctx->warnTemp = 30.0f;
  ctx->critTemp = 32.0f;

  // create semaphores/mutexes
  ctx->tempSem = xSemaphoreCreateBinary();
  ctx->humidSem = xSemaphoreCreateBinary();
  ctx->displaySem = xSemaphoreCreateBinary();
  ctx->publishSem = xSemaphoreCreateBinary();
  ctx->ledNeoSem = xSemaphoreCreateMutex();
  ctx->neopixelMutex = xSemaphoreCreateMutex();
  ctx->lcdMutex = xSemaphoreCreateMutex();
  ctx->sensorMutex = xSemaphoreCreateMutex();
  ctx->neoSem = xSemaphoreCreateBinary();
  ctx->controlSem = xSemaphoreCreateBinary();
  // create queues
  ctx->displayQueue = xQueueCreate(4, sizeof(DisplayState));
  ctx->controlQueue = xQueueCreate(8, sizeof(ControlMsg));

  // initial flags
  ctx->overrideLed = false;
  ctx->overrideNeo = false;
  ctx->overrideRelay = false;

  ctx->neo_r = ctx->neo_g = ctx->neo_b = 0;

  // initialize modules (sensors, lcd, neo, mqtt)
  led_neo_init(ctx);  // begin pixels_task1
  dht_lcd_init(ctx);   // begins Wire, dht, lcd
  neo_init(ctx);       // begin neopixels
  mqtt_init(ctx);      // create mqtt client objects (does not connect yet)

  // wifi AP + attempt STA if provided
  // --- Start AP (always) and print AP IP so user can connect and configure STA ---
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP started: %s\n", AP_SSID);
  Serial.printf("AP IP: %s\n", apIP.toString().c_str());
  Serial.printf("Connect to SSID '%s' (password '%s') and open http://%s/ to configure WiFi\n",
                AP_SSID, AP_PASS, apIP.toString().c_str());

  // --- Try STA if a default is provided in config or if user already filled ctx->wifiSsid
  // If neither exists, we skip STA attempt and let web UI (/wifi) collect credentials.
  if ( (ctx->wifiSsid.length() > 0) || (strlen(DEFAULT_WLAN_SSID) > 0) ) {
    const char* trySsid = nullptr;
    const char* tryPass = nullptr;
    if (ctx->wifiSsid.length() > 0) {
      trySsid = ctx->wifiSsid.c_str();
      tryPass = ctx->wifiPass.c_str();
      Serial.printf("[WIFI] Attempting STA using saved credentials from context: %s\n", trySsid);
    } else {
      trySsid = DEFAULT_WLAN_SSID;
      tryPass = DEFAULT_WLAN_PASS;
      Serial.printf("[WIFI] Attempting STA using DEFAULT_WLAN_SSID: %s\n", trySsid);
    }

    WiFi.begin(trySsid, tryPass);
    Serial.print("Trying STA connect");
    // short blocking attempt (non-blocking overall app, just quick try)
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; ++i) {
      Serial.print(".");
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("STA IP: %s (connected to %s)\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
    } else {
      Serial.println("STA not connected (will retry in background tasks or use web UI to set credentials).");
    }
  } else {
    Serial.println("No STA configured (DEFAULT_WLAN_SSID empty and ctx->wifiSsid not set). Waiting for user to configure via AP web UI.");
  }


  // relay pin init
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // setup web routes (task4) - MUST be called before creating TaskServer
  web_setup_routes(ctx);

  // create tasks (pass ctx)
  // sensor / producer tasks
  xTaskCreate(TaskTemperatureHumidity, "TaskTemp", 4096, ctx, 5, NULL);
  // actuators & controllers
  xTaskCreate(TaskBlink, "TaskBlink", 2048, ctx, 3, NULL);
  xTaskCreate(TaskNeoPixel, "TaskNeo", 3072, ctx, 3, NULL);
  xTaskCreate(TaskNeoController, "TaskNeoCtrl", 2048, ctx, 3, NULL);
  xTaskCreate(TaskLCDDisplay, "TaskLCD", 3072, ctx, 4, NULL);
  xTaskCreate(TaskLight,"TaskLight", 2048, ctx, 2, NULL);
  xTaskCreate(TaskMoisture,"TaskMoisture",2048, ctx, 2, NULL);
  xTaskCreate(TaskTinyML,"TaskTinyML", 8192, ctx,2,NULL);
  // control consumer & networking
  xTaskCreate(TaskControlConsumer, "TaskControl", 3072, ctx, 2, NULL);
  xTaskCreate(TaskCoreIOT, "TaskCoreIOT", 4096, ctx, 2, NULL);
  // web server handler
  xTaskCreate(TaskServer, "TaskServer", 4096, ctx, 1, NULL);


  Serial.println("Setup complete.");
}

void loop() {
  // everything runs in FreeRTOS tasks
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("STA IP: %s (connected to %s)\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
  } else {
    // show AP IP so user knows where to hit the web UI
    Serial.printf("AP IP: %s  (AP SSID: %s)\n", WiFi.softAPIP().toString().c_str(), AP_SSID);
  }
  vTaskDelay(pdMS_TO_TICKS(10000));
}
