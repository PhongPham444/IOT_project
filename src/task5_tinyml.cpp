#include "task5_tinyml.h"


// Globals for the TF Lite runtime (one-shot setup)
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 8 * 1024; // Adjust size based on your model
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

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

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

        // If we were given a valid context, read analog snapshot under sensorMutex to get consistent values
        if (ctx && ctx->sensorMutex) {
            if (xSemaphoreTake(ctx->sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                a0 = ctx->latestLight;
                a1 = ctx->latestMoisture;
                xSemaphoreGive(ctx->sensorMutex);
            }
        }

        // Prepare input data for model (A0, A1)
        // Model expects floats in input tensor; adjust indexing to your model's input shape
        input->data.f[0] = a0;
        input->data.f[1] = a1;

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
        float result = output->data.f[0];
        Serial.print("Inference result: ");
        Serial.println(result);
        digitalWrite(PIN_LED, result > 0.45 ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}