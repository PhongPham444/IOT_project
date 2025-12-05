#include "system_context.h"
#include "config.h"
#include "light.h"
#include "moisture.h"

void TaskLight(void *parameter){
    SystemContext* ctx = (SystemContext*) parameter;
    while (1){
        int raw0 = analogRead(PIN_LIGHT);
        float light = raw0/7000.0f;
        Serial.print("Light raw: ");
        Serial.print(raw0);
        Serial.print(" normalized: ");
        Serial.println(light, 4);

        // publish snapshot if context available
        if (ctx && ctx->sensorMutex) {
            if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                ctx->latestLight = light;
                xSemaphoreGive(ctx->sensorMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskMoisture(void *parameter){
    SystemContext* ctx = (SystemContext*) parameter;
    while (1){
        int raw1 = analogRead(PIN_MOISTURE);
        float moisture = (raw1 +100)/3000.0f; // keep original scaling
        Serial.print("Moisture raw: ");
        Serial.print(raw1);
        Serial.print(" normalized: ");
        Serial.println(moisture, 4);

        if (ctx && ctx->sensorMutex) {
            if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                ctx->latestMoisture = moisture;
                xSemaphoreGive(ctx->sensorMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}