#include "task2_neopixel.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ArduinoJson.h>

/* Helper to create color using ctx->pixels->Color(...) */
static uint32_t make_color(SystemContext* ctx, uint8_t r, uint8_t g, uint8_t b) {
  if (!ctx || !ctx->pixels) return 0;
  return ctx->pixels->Color(r, g, b);
}

void neo_init(SystemContext* ctx) {
  if (!ctx || !ctx->pixels) return;
  ctx->pixels->begin();
  ctx->pixels->show(); // clear
}

void neo_safe_set(SystemContext* ctx, uint32_t color) {
  if (!ctx || !ctx->pixels) return;
  if (xSemaphoreTake(ctx->neopixelMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    for (int i = 0; i < NEO_COUNT; ++i) ctx->pixels->setPixelColor(i, color);
    ctx->pixels->show();
    xSemaphoreGive(ctx->neopixelMutex);
  } else {
    // mutex busy -> skip
  }
}

/*
 TaskNeoPixel: Auto behavior based on humidity (Task 2)
 - humidity ranges:
   <=40 -> BLUE steady
   41..60 -> GREEN steady
   61..80 -> YELLOW blinking (warning)
   >80 -> RED fast blinking (critical)
 - Uses humidSem to be signaled by sensor task.
 - Sends telemetry via ctx->mqttClient (if connected) for warning/critical.
*/
void TaskNeoPixel(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  const TickType_t blinkFast = pdMS_TO_TICKS(150);
  const TickType_t blinkSlow = pdMS_TO_TICKS(500);
  bool blinkState = false;

  for (;;) {
    if (ctx->overrideNeo) { // manual override from web
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // wait for humidSem (producer gives)
    if (xSemaphoreTake(ctx->humidSem, portMAX_DELAY) == pdTRUE) {
      float h = 0.0f;
      if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        h = ctx->latestHumid;
        xSemaphoreGive(ctx->sensorMutex);
      }

      if (h <= 40.0f) {
        neo_safe_set(ctx, make_color(ctx, 0, 0, 255)); // blue
      } else if (h <= 60.0f) {
        neo_safe_set(ctx, make_color(ctx, 0, 255, 0)); // green
      } else if (h <= 80.0f) {
        blinkState = !blinkState;
        if (blinkState) neo_safe_set(ctx, make_color(ctx, 255, 200, 0));
        else neo_safe_set(ctx, make_color(ctx, 0,0,0));
        // publish warning telemetry (non-blocking)
        if (ctx->mqttClient && ctx->mqttClient->connected()) {
          DynamicJsonDocument d(128); d["type"]="warning"; d["humidity"]=h;
          String payload; serializeJson(d, payload);
          ctx->mqttClient->publish("v1/devices/me/telemetry", payload.c_str());
        }
        vTaskDelay(blinkSlow);
      } else {
        blinkState = !blinkState;
        if (blinkState) neo_safe_set(ctx, make_color(ctx, 255,0,0));
        else neo_safe_set(ctx, make_color(ctx, 0,0,0));
        if (ctx->mqttClient && ctx->mqttClient->connected()) {
          DynamicJsonDocument d(128); d["type"]="critical"; d["humidity"]=h;
          String payload; serializeJson(d, payload);
          ctx->mqttClient->publish("v1/devices/me/telemetry", payload.c_str());
        }
        vTaskDelay(blinkFast);
      }
    }
  }
}

/*
 TaskNeoController: waits on neoSem and applies ctx->neo_r/g/b immediately (manual group control).
*/
void TaskNeoController(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  for (;;) {
    if (xSemaphoreTake(ctx->neoSem, portMAX_DELAY) == pdTRUE) {
      uint8_t r = ctx->neo_r, g = ctx->neo_g, b = ctx->neo_b;
      uint32_t color = make_color(ctx, r,g,b);
      if (xSemaphoreTake(ctx->neopixelMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        for (int i=0;i<NEO_COUNT;i++) ctx->pixels->setPixelColor(i, color);
        ctx->pixels->show();
        xSemaphoreGive(ctx->neopixelMutex);
      }
      Serial.printf("[NeoCtrl] manual color %u,%u,%u applied\n", r,g,b);
    }
  }
}
