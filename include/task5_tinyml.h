#ifndef __TINY_ML__
#define __TINY_ML__

#include <Arduino.h>

#include "water_model.h"
#include "config.h"

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <tensorflow/lite/micro/all_ops_resolver.h>

// The TinyML task expects a pointer to the shared SystemContext (passed as pvParameters)
#include "system_context.h"

void setupTinyML();
void TaskTinyML(void *pvParameters);

#endif  