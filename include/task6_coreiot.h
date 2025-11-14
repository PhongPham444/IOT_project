#pragma once
#include "system_context.h"

void mqtt_init(SystemContext* ctx);
void mqtt_ensure_connected(SystemContext* ctx);
void mqtt_loop(SystemContext* ctx);
void mqtt_publish_telemetry(SystemContext* ctx, float t, float h);
void TaskCoreIOT(void* pv);