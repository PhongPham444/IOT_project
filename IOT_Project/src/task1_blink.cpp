#include "task1_blink.h"
#include "config.h"
#include <Arduino.h>

/*
 Task 1: Single LED Blink with Temperature Conditions
 - Uses tempSem to get notified of new temperature readings.
 - Uses sensorMutex to read latestTemp snapshot.
 - Supports overrideLed via control queue (web).
 - No global variables; all through SystemContext.
*/

static uint32_t period_ms_for_state(DisplayState s) {
  if (s == STATE_NORMAL) return 1000;
  if (s == STATE_WARNING) return 400;
  return 150; // critical
}

void TaskBlink(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  pinMode(PIN_LED_BLINK, OUTPUT);
  digitalWrite(PIN_LED_BLINK, LOW);

  DisplayState curState = STATE_NORMAL;

  for (;;) {
    // if web override, simply respect overrideLed and sleep
    if (ctx->overrideLed) {
      digitalWrite(PIN_LED_BLINK, ctx->overrideLed ? HIGH : LOW);
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    // Drain any tempSem signals (non-blocking) to refresh curState
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

    // blink on
    digitalWrite(PIN_LED_BLINK, HIGH);
    vTaskDelay(pdMS_TO_TICKS(period_ms_for_state(curState) / 2));

    // check for quick updates while waiting
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

    // blink off
    digitalWrite(PIN_LED_BLINK, LOW);
    vTaskDelay(pdMS_TO_TICKS(period_ms_for_state(curState) / 2));
  }
}
