#pragma once
#include "system_context.h"

void mqtt_init(SystemContext* ctx);
void mqtt_ensure_connected(SystemContext* ctx);
void mqtt_loop(SystemContext* ctx);
void mqtt_publish_telemetry(SystemContext* ctx, float t, float h);
// MQTT callback for incoming messages (LED control)
void mqtt_on_message(char* topic, byte* payload, unsigned int length, SystemContext* ctx);
// Subscribe to LED control topic
void mqtt_subscribe_led_control(SystemContext* ctx);
void TaskCoreIOT(void* pv);