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
  
  // Set MQTT callback for incoming RPC messages from CoreIOT
  // Using lambda to pass context to callback (PubSubClient doesn't support context directly)
  ctx->mqttClient->setCallback([ctx](char* topic, byte* payload, unsigned int length) {
    mqtt_on_message(topic, payload, length, ctx);
  });
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

// MQTT message callback for handling incoming RPC commands from CoreIOT
// Follows CoreIOT RPC pattern: {"method": "setValue", "params": true/false}
void mqtt_on_message(char* topic, byte* payload, unsigned int length, SystemContext* ctx) {
  if (!ctx) return;
  
  // Parse JSON payload: expect {"method": "setValue", "params": true/false}
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.printf("[MQTT] JSON parse error: %s\n", error.c_str());
    return;
  }

  // Check if this is a setValue RPC method
  if (doc.containsKey("method") && strcmp(doc["method"], "setValue") == 0) {
    if (doc.containsKey("params")) {
      bool ledOn = doc["params"].as<bool>();
      
      // Update CoreIOT LED state under mutex
      if (ctx->sensorMutex) {
        if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          ctx->coreiotLedOn = ledOn;
          xSemaphoreGive(ctx->sensorMutex);
        }
      }
      
      // Apply LED control immediately
      digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
      Serial.printf("[MQTT] LED control: %s\n", ledOn ? "ON" : "OFF");
      
      // Publish response attributes back to CoreIOT (confirm state)
      if (ctx->mqttClient && ctx->mqttClient->connected()) {
        DynamicJsonDocument resp(128);
        resp["value"] = ledOn;
        String respPayload;
        serializeJson(resp, respPayload);
        ctx->mqttClient->publish("v1/devices/me/attributes", respPayload.c_str());
        Serial.printf("[MQTT] published LED state response: %s\n", respPayload.c_str());
      }
      
      // Signal Task 5 that LED state has changed via CoreIOT
      if (ctx->ledControlSem) {
        xSemaphoreGive(ctx->ledControlSem);
        Serial.println("[MQTT] signaled ledControlSem for Task 5");
      }
    }
  }
}

// Subscribe to RPC commands topic on CoreIOT (generic, for all methods)
void mqtt_subscribe_led_control(SystemContext* ctx) {
  if (!ctx || !ctx->mqttClient) return;
  
  // Subscribe to generic RPC topic (matches CoreIOT Python: v1/devices/me/rpc/request/+)
  const char* topic = "v1/devices/me/rpc/request/+";
  bool ok = ctx->mqttClient->subscribe(topic);
  Serial.printf("[MQTT] subscribe to RPC topic: %s (ok=%d)\n", topic, ok);
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
      
      // Subscribe to LED control topic after connecting
      static bool ledSubScribed = false;
      if (!ledSubScribed && ctx->mqttClient->connected()) {
        mqtt_subscribe_led_control(ctx);
        ledSubScribed = true;
      }

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
