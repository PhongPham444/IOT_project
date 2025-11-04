#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <DHT20.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <Preferences.h>
typedef enum { STATE_NORMAL = 0, STATE_WARNING, STATE_CRITICAL } DisplayState;

typedef struct {
  // Hardware objects (allocated in main)
  Adafruit_NeoPixel* pixels_task1;
  Adafruit_NeoPixel* pixels;
  DHT20* dht;
  LiquidCrystal_I2C* lcd;
  WebServer* server;

  // Network/mqtt clients (allocated in main)
  WiFiClient* wifiClient;
  PubSubClient* mqttClient;

  // synchronization primitives
  SemaphoreHandle_t tempSem;
  SemaphoreHandle_t humidSem;
  SemaphoreHandle_t displaySem;
  SemaphoreHandle_t publishSem;
  SemaphoreHandle_t neopixelMutex;
  SemaphoreHandle_t lcdMutex;
  SemaphoreHandle_t sensorMutex;
  SemaphoreHandle_t neoSem;
  SemaphoreHandle_t ledNeoSem;
  // new: semaphore to indicate control command available
  SemaphoreHandle_t controlSem;

  // queues
  QueueHandle_t displayQueue;
  QueueHandle_t controlQueue;

  // runtime snapshot (no globals)
  float latestTemp;
  float latestHumid;

  // thresholds
  float warnTemp;
  float critTemp;

  // override flags (state)
  bool overrideLed;
  bool overrideNeo;
  bool overrideRelay;

  // manual neo color
  uint8_t neo_r;
  uint8_t neo_g;
  uint8_t neo_b;

  // saved wifi from web form
  String wifiSsid;
  String wifiPass;

  // coreiot token if needed
  String coreToken;


} SystemContext;

typedef struct {
  uint8_t device; // 0: all off,1: led blink toggle,2: neo toggle (not used),3: relay toggle,4: toggle blink.
  bool on;
  int param;
} ControlMsg;
