#include "task3_dht_lcd.h"
#include <Wire.h>
#include <Arduino.h>

/*
 Task 3: Temperature and Humidity Monitoring + LCD (display states)
 - Producer TaskTemperatureHumidity: reads DHT20, updates snapshot (with sensorMutex),
   gives tempSem/humidSem and sends display state to displayQueue + displaySem.
 - TaskLCDDisplay: consumes displayQueue/displaySem and draws 3 display states.
 - No global variables; everything via ctx.
*/

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

    // build display state
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

void TaskLCDDisplay(void* pv) {
  SystemContext* ctx = (SystemContext*) pv;
  if (!ctx) vTaskDelete(NULL);

  DisplayState lastState = STATE_NORMAL;
  bool blinkOn = false;
  vTaskDelay(pdMS_TO_TICKS(100));

  for (;;) {
    if (!ctx->lcd) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }

    if (ctx->displaySem) {
      if (xSemaphoreTake(ctx->displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        DisplayState s;
        // drain queue -> get latest
        while (xQueueReceive(ctx->displayQueue, &s, 0) == pdTRUE) lastState = s;
      }
      // else timeout -> keep lastState
    } else {
      DisplayState s;
      if (xQueueReceive(ctx->displayQueue, &s, pdMS_TO_TICKS(10)) == pdTRUE) lastState = s;
    }

    uint32_t period_ms = 1000;
    if (lastState == STATE_WARNING) period_ms = 500;
    else if (lastState == STATE_CRITICAL) period_ms = 150;

    float t = 0.0f, h = 0.0f;
    if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      t = ctx->latestTemp; h = ctx->latestHumid;
      xSemaphoreGive(ctx->sensorMutex);
    }

    if (xSemaphoreTake(ctx->lcdMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      ctx->lcd->clear();
      ctx->lcd->setCursor(0,0);
      char buf[32];
      snprintf(buf, sizeof(buf), "T:%.1fC H:%.1f%%", t, h);
      ctx->lcd->print(buf);
      ctx->lcd->setCursor(0,1);
      if (lastState == STATE_NORMAL) ctx->lcd->print("State: NORMAL");
      else if (lastState == STATE_WARNING) ctx->lcd->print(blinkOn ? "!! WARNING !!" : "  WARNING   ");
      else ctx->lcd->print(blinkOn ? "!! CRITICAL !!" : " CRITICAL  ");
      xSemaphoreGive(ctx->lcdMutex);
    }

    if (lastState != STATE_NORMAL) blinkOn = !blinkOn;
    else blinkOn = false;

    vTaskDelay(pdMS_TO_TICKS(period_ms / 2));
  }
}
