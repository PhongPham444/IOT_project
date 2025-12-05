// src/task1_blink.cpp
#include "task1_blink.h"
#include "system_context.h"
#include "config.h"
#include <Arduino.h>

/*
 Task1 (NeoPixel, temperature-based)
 - Uses ctx->pixels_task1 (Adafruit_NeoPixel*) on PIN_LED_BLINK with LED_COUNT LEDs (usually 1)
 - Synchronizes via ctx->ledNeoSem (created as mutex in main)
 - Uses ctx->tempSem and ctx->sensorMutex to read latestTemp snapshot
 - Respects ctx->overrideNeo (manual override from web) — when true, automatic patterns are suppressed
 - Provides helper neo_safe_set_task1(ctx, color)
*/

/// Helper: safely set color for pixels_task1 (takes ctx->ledNeoSem mutex)
void led_neo_init(SystemContext* ctx) {
  if (!ctx || !ctx->pixels_task1) return;
  ctx->pixels_task1->begin();
  ctx->pixels_task1->show(); // clear
}
static void neo_safe_set_task1(SystemContext* ctx, uint32_t color) {
  if (!ctx || !ctx->pixels_task1) return;
  if (!ctx->ledNeoSem) {
    // no mutex available: attempt to set directly (best-effort)
    for (int i = 0; i < LED_COUNT; ++i) ctx->pixels_task1->setPixelColor(i, color);
    ctx->pixels_task1->show();
    return;
  }
  if (xSemaphoreTake(ctx->ledNeoSem, pdMS_TO_TICKS(200)) == pdTRUE) {
    for (int i = 0; i < LED_COUNT; ++i) ctx->pixels_task1->setPixelColor(i, color);
    ctx->pixels_task1->show();
    xSemaphoreGive(ctx->ledNeoSem);
  } else {
    // mutex busy, skip this update (non-fatal)
    Serial.println("[Task1] ledNeoSem busy - skipping update");
  }
}

// We can't call Adafruit_NeoPixel::Color without ctx->pixels_task1; so we'll create inline helper macro-like:
static inline uint32_t np_color(SystemContext* ctx, uint8_t r, uint8_t g, uint8_t b) {
  // Use the instance's Color method if available; else pack RGB manually as GRB may differ,
  // but usually Adafruit_NeoPixel::Color is static-like and can be called using any instance.
  if (ctx && ctx->pixels_task1) return ctx->pixels_task1->Color(r, g, b);
  // fallback: pack as (r<<16)|(g<<8)|b - may not match strip order but is fallback
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint32_t period_ms_for_state(DisplayState s) {
  if (s == STATE_NORMAL) return 1000;
  if (s == STATE_WARNING) return 500;
  return 150;
}

void TaskBlink(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  if (!ctx->pixels_task1) {
    Serial.println("[Task1] pixels_task1 not initialized - deleting task.");
    vTaskDelete(NULL);
    return;
  }

  // Ensure LED(s) off initially
  neo_safe_set_task1(ctx, np_color(ctx, 0, 0, 0));

  DisplayState curState = STATE_NORMAL;
  bool blinkOn = false;

  for (;;) {
    // If manual override for Neo is active, yield and don't run auto patterns
    if (ctx->overrideLed) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    // Drain tempSem signals to update curState (non-blocking)
    while (xSemaphoreTake(ctx->tempSem, 0) == pdTRUE) {
      float t = 0.0f;
      if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        t = ctx->latestTemp;
        xSemaphoreGive(ctx->sensorMutex);
      }
      if (t >= ctx->critTemp) curState = STATE_CRITICAL;
      else if (t >= ctx->warnTemp) curState = STATE_WARNING;
      else curState = STATE_NORMAL;
    }

    if (curState == STATE_NORMAL) {
      // steady green
      neo_safe_set_task1(ctx, np_color(ctx, 0, 255, 0));
      // sleep a period, but still be responsive to tempSem (we sleep shorter slices)
      // we'll sleep in 200ms chunks and check for tempSem quickly
      uint32_t total = period_ms_for_state(curState);
      uint32_t slept = 0;
      while (slept < total) {
        // if new temp update arrives, break to re-evaluate quickly
        if (xSemaphoreTake(ctx->tempSem, 0) == pdTRUE) {
          float t = 0.0f;
          if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            t = ctx->latestTemp; xSemaphoreGive(ctx->sensorMutex);
          }
          if (t >= ctx->critTemp) curState = STATE_CRITICAL;
          else if (t >= ctx->warnTemp) curState = STATE_WARNING;
          else curState = STATE_NORMAL;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        slept += 200;
      }
      blinkOn = false;
      continue;
    }

    if (curState == STATE_WARNING) {
      // yellow slow blink
      blinkOn = !blinkOn;
      if (blinkOn) neo_safe_set_task1(ctx, np_color(ctx, 255, 180, 0));
      else neo_safe_set_task1(ctx, np_color(ctx, 0, 0, 0));
      // allow quick responds to new temp events while blinking
      uint32_t half = period_ms_for_state(curState) / 2;
      uint32_t slept = 0;
      while (slept < half) {
        if (xSemaphoreTake(ctx->tempSem, 0) == pdTRUE) {
          float t = 0.0f;
          if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            t = ctx->latestTemp; 
            xSemaphoreGive(ctx->sensorMutex);
          }
          if (t >= ctx->critTemp) curState = STATE_CRITICAL;
          else if (t >= ctx->warnTemp) curState = STATE_WARNING;
          else curState = STATE_NORMAL;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        slept += 50;
      }
      continue;
    }

    // STATE_CRITICAL
    blinkOn = !blinkOn;
    if (blinkOn) neo_safe_set_task1(ctx, np_color(ctx, 255, 0, 0));
    else neo_safe_set_task1(ctx, np_color(ctx, 0, 0, 0));
    uint32_t halfcrit = period_ms_for_state(curState) / 2;
    uint32_t sleptc = 0;
    while (sleptc < halfcrit) {
      if (xSemaphoreTake(ctx->tempSem, 0) == pdTRUE) {
        float t = 0.0f;
        if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          t = ctx->latestTemp; 
          xSemaphoreGive(ctx->sensorMutex);
        }
        if (t >= ctx->critTemp) curState = STATE_CRITICAL;
        else if (t >= ctx->warnTemp) curState = STATE_WARNING;
        else curState = STATE_NORMAL;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      sleptc += 50;
    }
    // loop
  } // end for
}
