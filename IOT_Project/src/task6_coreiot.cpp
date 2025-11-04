// src/task6_coreiot.cpp
#include "task6_coreiot.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "config.h"

/*
  Task6 - CoreIOT MQTT publish
  - mqtt_publish_telemetry now includes "longitude" and "latitude"
  - Uses ctx->mqttClient (PubSubClient)
*/

void mqtt_init(SystemContext* ctx) {
  if (!ctx) return;
  if (!ctx->wifiClient) ctx->wifiClient = new WiFiClient();
  if (!ctx->mqttClient) ctx->mqttClient = new PubSubClient(*ctx->wifiClient);
  ctx->mqttClient->setServer(COREIOT_MQTT_HOST, COREIOT_MQTT_PORT);
  ctx->coreToken = String(COREIOT_MQTT_PASS);
}

void mqtt_ensure_connected(SystemContext* ctx) {
  if (!ctx || !ctx->mqttClient) return;
  if (ctx->mqttClient->connected()) return;
  Serial.printf("[MQTT] connect to %s:%u ...\n", COREIOT_MQTT_HOST, COREIOT_MQTT_PORT);
  const char* passPtr = (COREIOT_MQTT_PASS && strlen(COREIOT_MQTT_PASS) > 0) ? COREIOT_MQTT_PASS : nullptr;
  if (ctx->mqttClient->connect(COREIOT_CLIENT_ID, COREIOT_MQTT_USER, passPtr)) {
    Serial.println("[MQTT] connected");
  } else {
    Serial.printf("[MQTT] connect failed rc=%d\n", ctx->mqttClient->state());
  }
}

void mqtt_loop(SystemContext* ctx) {
  if (!ctx || !ctx->mqttClient) return;
  ctx->mqttClient->loop();
}

void mqtt_publish_telemetry(SystemContext* ctx, float t, float h) {
  if (!ctx || !ctx->mqttClient) return;
  if (!ctx->mqttClient->connected()) {
    mqtt_ensure_connected(ctx);
    if (!ctx->mqttClient->connected()) { Serial.println("[MQTT] not connected, skip publish"); return; }
  }

  // increase JSON buffer slightly to hold extra numeric fields
  DynamicJsonDocument doc(512);
  doc["temperature"] = t;
  doc["humidity"] = h;
  // add location fields (double precision)
  doc["longitude"] = DEVICE_LONGITUDE;
  doc["latitude"]  = DEVICE_LATITUDE;

  String payload;
  serializeJson(doc, payload);

  const char* topic = "v1/devices/me/telemetry";
  bool ok = ctx->mqttClient->publish(topic, payload.c_str());
  Serial.printf("[MQTT] publish telemetry ok=%d payload=%s\n", ok, payload.c_str());
}

/* ---------------- TaskCoreIOT ----------------
   Runs periodic publish and reacts to ctx->publishSem
*/
void TaskCoreIOT(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  const TickType_t period = pdMS_TO_TICKS(5000);

  for (;;) {
    BaseType_t sig = pdFALSE;
    if (ctx->publishSem) sig = xSemaphoreTake(ctx->publishSem, period);
    else vTaskDelay(period);

    if (sig == pdTRUE) Serial.println("[CoreIOT] publish triggered by web");
    else Serial.println("[CoreIOT] periodic publish");

    // ensure STA connection: try saved creds then default
    if (WiFi.status() != WL_CONNECTED) {
      if (ctx->wifiSsid.length() > 0) {
        Serial.printf("[CoreIOT] Trying saved STA %s\n", ctx->wifiSsid.c_str());
        WiFi.begin(ctx->wifiSsid.c_str(), ctx->wifiPass.c_str());
        for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; ++i) vTaskDelay(pdMS_TO_TICKS(200));
      } else if (strlen(DEFAULT_WLAN_SSID) > 0) {
        Serial.printf("[CoreIOT] Trying DEFAULT STA %s\n", DEFAULT_WLAN_SSID);
        WiFi.begin(DEFAULT_WLAN_SSID, DEFAULT_WLAN_PASS);
        for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; ++i) vTaskDelay(pdMS_TO_TICKS(200));
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      mqtt_ensure_connected(ctx);
      mqtt_loop(ctx);

      float t = 0.0f, h = 0.0f;
      if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        t = ctx->latestTemp; h = ctx->latestHumid;
        xSemaphoreGive(ctx->sensorMutex);
      }

      mqtt_publish_telemetry(ctx, t, h);
    } else {
      Serial.println("[CoreIOT] STA not connected - skipping publish");
    }
  }
}
