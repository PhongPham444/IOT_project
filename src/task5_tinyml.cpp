#include "task5_tinyml.h"


// Globals for the TF Lite runtime (one-shot setup)
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 16 * 1024; // Increased from 8KB to 16KB
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(water_model_tflite); // g_model_data is from model_data.h
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    Serial.printf("[TinyML] Model schema version: %d (expected %d)\n", 
                  model->version(), TFLITE_SCHEMA_VERSION);

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        Serial.println("[TinyML ERROR] AllocateTensors() failed! Increase kTensorArenaSize");
        return;
    }
    Serial.println("[TinyML] AllocateTensors() succeeded");

    input = interpreter->input(0);
    output = interpreter->output(0);

    // Verify tensor dimensions
    Serial.printf("[TinyML] Input tensor: %d dims, shape: ", input->dims->size);
    for (int i = 0; i < input->dims->size; i++) {
        Serial.printf("%d ", input->dims->data[i]);
    }
    Serial.printf("\n[TinyML] Output tensor: %d dims, shape: ", output->dims->size);
    for (int i = 0; i < output->dims->size; i++) {
        Serial.printf("%d ", output->dims->data[i]);
    }
    Serial.println();

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

void TaskTinyML(void *pvParameters)
{
    // pvParameters is expected to be a pointer to SystemContext
    SystemContext* ctx = (SystemContext*) pvParameters;

    setupTinyML();

    while (1)
    {
        float a0 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        bool coreiotLedState = false;

        // TEST: Use exact same values as notebook to verify model works
        // If we were given a valid context, read analog snapshot under sensorMutex to get consistent values
        if (ctx && ctx->sensorMutex) {
            if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                // Use exact notebook test values: [20, 53.0, 0.3, 0.18]
                // a2 = 29.0f;      // Temperature (matches notebook)
                // a3 = 53.0f;      // Humidity (matches notebook)
                // a1 = 0.3f;       // Moisture (matches notebook)
                // a0 = 0.18f;      // Light (matches notebook)
                
                // UNCOMMENT BELOW TO USE REAL SENSOR VALUES:
                a0 = ctx->latestLight;
                a1 = ctx->latestMoisture;
                a2 = ctx->latestTemp;
                a3 = ctx->latestHumid;
                
                coreiotLedState = ctx->coreiotLedOn;
                xSemaphoreGive(ctx->sensorMutex);
            }
        }

        // Debug: Print input values
        Serial.printf("[TinyML] Inputs - Temp: %.2f, Humid: %.2f, Moisture: %.2f, Light: %.2f\n", 
                      a2, a3, a1, a0);

        // Verify input tensor is valid
        if (input == nullptr || input->data.f == nullptr) {
            Serial.println("[TinyML ERROR] Input tensor is NULL!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        Serial.printf("[TinyML] Input tensor type: %d, bytes: %d\n", input->type, input->bytes);

        input->data.f[0] = a2;
        input->data.f[1] = a3;
        input->data.f[2] = a1;
        input->data.f[3] = a0;


        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            // keep task alive but wait a bit before retrying
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        // Get and process output
        // float result = (output->data.f[0] > 0.6) ? 1.0f : 0.0f;
        
        // Verify output tensor is valid
        if (output == nullptr || output->data.f == nullptr) {
            Serial.println("[TinyML ERROR] Output tensor is NULL!");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        Serial.printf("[TinyML] Output tensor type: %d, bytes: %d\n", output->type, output->bytes);
        
        float result = output->data.f[0];

        // Debug: Check output validity
        if (isnan(result) || isinf(result)) {
            Serial.printf("[TinyML ERROR] Output is NaN or Inf! Inputs were: T=%.2f H=%.2f M=%.2f L=%.2f\n",
                         a2, a3, a1, a0);
            Serial.printf("[TinyML] Raw output value: %f (0x%08X)\n", result, *(uint32_t*)&result);
            result = 0.0f; // Default safe value
        } else {
            Serial.printf("[TinyML] Inference result: %.4f\n", result);
        }
        
        // Store result in context for web display
        if (ctx && ctx->sensorMutex) {
            if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                ctx->latestAIOutput = result;
                xSemaphoreGive(ctx->sensorMutex);
            }
        }
        
        // Apply LED: prioritize CoreIOT control over inference if enabled
        bool ledFinal = coreiotLedState ? coreiotLedState : (result > 0.4);
        digitalWrite(PIN_LED, ledFinal ? HIGH : LOW);
        
        // Optional: wait for CoreIOT LED control semaphore to react to MQTT changes
        // This provides reactive behavior when LED is controlled from CoreIOT
        if (ctx && ctx->ledControlSem) {
            xSemaphoreTake(ctx->ledControlSem, pdMS_TO_TICKS(300));
        } else {
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
}