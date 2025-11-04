// src/task3_dht_lcd.cpp
#include "task3_dht_lcd.h"
#include <Wire.h>
#include <Arduino.h>

/*
 Task 3: Temperature and Humidity Monitoring + LCD (display states)
 - Displays both temperature state and humidity state on the 2nd LCD row in format:
     TEM:NOR HUM:CRI   (max 16 chars)
 - Uses ctx->warnTemp / ctx->critTemp for temperature thresholds.
 - Uses local WARN_HUM / CRIT_HUM thresholds for humidity (can be moved to ctx if desired).
 - Uses sensorMutex/lcdMutex for safe access.
*/

static const float WARN_HUM = 60.0f;   // warning threshold for humidity
static const float CRIT_HUM = 80.0f;   // critical threshold for humidity

void dht_lcd_init(SystemContext* ctx) {
  if (!ctx) return;
  // initialize I2C (pins chosen as in original)
  Wire.begin(GPIO_NUM_11, GPIO_NUM_12);
  if (ctx->dht) ctx->dht->begin();
  if (ctx->lcd) ctx->lcd->begin();
}

void TaskTemperatureHumidity(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  const TickType_t poll = pdMS_TO_TICKS(2000);
  DisplayState lastState = STATE_NORMAL;

  for (;;) {
    if (ctx->dht) ctx->dht->read();
    float t = ctx->dht ? ctx->dht->getTemperature() : 0.0f;
    float h = ctx->dht ? ctx->dht->getHumidity() : 0.0f;

    Serial.printf("[Sensor] T: %.2f C, H: %.2f %%\n", t, h);

    // update snapshot atomically
    if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      ctx->latestTemp = t;
      ctx->latestHumid = h;
      xSemaphoreGive(ctx->sensorMutex);
    }

    // signal semaphores for tasks that wait
    xSemaphoreGive(ctx->tempSem);
    xSemaphoreGive(ctx->humidSem);

    // build temp-only display state (kept for compatibility)
    DisplayState newState;
    if (t >= ctx->critTemp) newState = STATE_CRITICAL;
    else if (t >= ctx->warnTemp) newState = STATE_WARNING;
    else newState = STATE_NORMAL;

    if (newState != lastState) {
      lastState = newState;
      xQueueSend(ctx->displayQueue, &newState, 0);
      xSemaphoreGive(ctx->displaySem);
    }

    vTaskDelay(poll);
  }
}

/* helper: map DisplayState -> 3-letter code */
static const char* abbrev_for_state(DisplayState s) {
  switch (s) {
    case STATE_NORMAL: return "NOR";
    case STATE_WARNING: return "WAR";
    case STATE_CRITICAL: return "CRI";
    default: return "UNK";
  }
}

/* Determine DisplayState from temperature value using ctx thresholds */
static DisplayState state_for_temp(SystemContext* ctx, float t) {
  if (t >= ctx->critTemp) return STATE_CRITICAL;
  if (t >= ctx->warnTemp) return STATE_WARNING;
  return STATE_NORMAL;
}

/* Determine DisplayState from humidity using local thresholds */
static DisplayState state_for_hum(float h) {
  if (h >= CRIT_HUM) return STATE_CRITICAL;
  if (h >= WARN_HUM) return STATE_WARNING;
  return STATE_NORMAL;
}

void TaskLCDDisplay(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  // we keep a small initial delay to allow other init to complete
  vTaskDelay(pdMS_TO_TICKS(100));

  bool blinkOn = false;

  for (;;) {
    if (!ctx->lcd) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // We still honor displaySem/displayQueue to be responsive to display-related events,
    // but the LCD will calculate both temp/hum states from latest snapshot.
    if (ctx->displaySem) {
      // Wait briefly for an update signal, but we will refresh periodically anyway
      xSemaphoreTake(ctx->displaySem, pdMS_TO_TICKS(200));
      // flush queue (not strictly necessary now)
      DisplayState tmp;
      while (xQueueReceive(ctx->displayQueue, &tmp, 0) == pdTRUE) { /* drain */ }
    }

    // Read snapshot atomically
    float t = 0.0f, h = 0.0f;
    if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      t = ctx->latestTemp;
      h = ctx->latestHumid;
      xSemaphoreGive(ctx->sensorMutex);
    }

    // compute states
    DisplayState tState = state_for_temp(ctx, t);
    DisplayState hState = state_for_hum(h);

    // Compose first line (T/H) - keep original format
    char line1[32];
    snprintf(line1, sizeof(line1), "T:%.1fC H:%.1f%%", t, h);

    // Compose second line with fixed format: "TEM:XXX HUM:YYY"
    // Ensure it's <=16 chars; our format yields 15 chars like "TEM:NOR HUM:CRI"
    char line2[17]; // 16 chars + null
    const char* tAb = abbrev_for_state(tState);
    const char* hAb = abbrev_for_state(hState);
    snprintf(line2, sizeof(line2), "TEM:%s HUM:%s", tAb, hAb);

    // Display to LCD under lcdMutex
    if (xSemaphoreTake(ctx->lcdMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      ctx->lcd->clear();
      ctx->lcd->setCursor(0,0);
      ctx->lcd->print(line1);

      ctx->lcd->setCursor(0,1);
      ctx->lcd->print(line2);

      xSemaphoreGive(ctx->lcdMutex);
    } else {
      Serial.println("[LCD] lcdMutex busy - skip update");
    }

    // For WARNING/CRITICAL we can toggle blinkOn for any extra behavior elsewhere.
    // Here we simply update at reasonable rate:
    // choose period based on worst state (if either is WARNING or CRITICAL, use faster period)
    uint32_t period_ms = 1000;
    if (tState == STATE_CRITICAL || hState == STATE_CRITICAL) period_ms = 150;
    else if (tState == STATE_WARNING || hState == STATE_WARNING) period_ms = 500;

    // toggle blink flag for potential blinking effects (not used in static line2)
    if (tState != STATE_NORMAL || hState != STATE_NORMAL) blinkOn = !blinkOn;
    else blinkOn = false;

    vTaskDelay(pdMS_TO_TICKS(period_ms));
  }
}
