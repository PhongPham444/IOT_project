// src/main.cpp
#include <Arduino.h>
#include "system_context.h"
#include "config.h"

// task headers
#include "task1_blink.h"
#include "task2_neopixel.h"
#include "task3_dht_lcd.h"
#include "task4_web.h"
#include "task6_coreiot.h"

// Note: Task5 (TinyML) left as placeholder

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

  // allocate hardware objects
  ctx->pixels = new Adafruit_NeoPixel(NEO_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
  ctx->dht = new DHT20();
  ctx->lcd = new LiquidCrystal_I2C(0x21, 16, 2);
  ctx->server = new WebServer(80);

  // network/mqtt pointers created in mqtt_init (called below)
  ctx->wifiClient = nullptr;
  ctx->mqttClient = nullptr;

  // default thresholds
  ctx->warnTemp = 30.0f;
  ctx->critTemp = 40.0f;

  // create semaphores/mutexes
  ctx->tempSem = xSemaphoreCreateBinary();
  ctx->humidSem = xSemaphoreCreateBinary();
  ctx->displaySem = xSemaphoreCreateBinary();
  ctx->publishSem = xSemaphoreCreateBinary();
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

  // fan initial state
  ctx->fanSpeed = 0;
  ctx->controlFan = false;

  // initialize modules (sensors, lcd, neo, mqtt)
  dht_lcd_init(ctx);   // begins Wire, dht, lcd
  neo_init(ctx);       // begin neopixels
  mqtt_init(ctx);      // create mqtt client objects (does not connect yet)

  // Fan pin: use analogWrite like your original sample (no ledc setup)
  pinMode(FAN_PIN, OUTPUT);
  analogWrite(FAN_PIN, 0); // ensure off

  // wifi AP + attempt STA if provided
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP started: %s\n", AP_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  if (strlen(DEFAULT_WLAN_SSID) > 0) {
    WiFi.begin(DEFAULT_WLAN_SSID, DEFAULT_WLAN_PASS);
    Serial.print("Trying STA connect");
    for (int i=0;i<20 && WiFi.status() != WL_CONNECTED; ++i) { Serial.print("."); delay(300); }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) Serial.printf("STA IP: %s\n", WiFi.localIP().toString().c_str());
    else Serial.println("STA not connected (TaskCoreIOT will retry).");
  }

  // relay pin init
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

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
  // control consumer & networking
  xTaskCreate(TaskControlConsumer, "TaskControl", 3072, ctx, 2, NULL);
  xTaskCreate(TaskCoreIOT, "TaskCoreIOT", 4096, ctx, 2, NULL);
  // web server handler
  xTaskCreate(TaskServer, "TaskServer", 4096, ctx, 1, NULL);

  Serial.println("Setup complete.");
}

void loop() {
  // everything runs in FreeRTOS tasks
  // print current IP every 10 seconds for quick monitoring
  Serial.printf("Current IP: %s\n", WiFi.localIP().toString().c_str());
  vTaskDelay(pdMS_TO_TICKS(10000));
}
