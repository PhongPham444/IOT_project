#pragma once
#include "system_context.h"

void TaskTemperatureHumidity(void* pv);
void TaskLCDDisplay(void* pv);

// helper to init DHT/LCD/I2C
void dht_lcd_init(SystemContext* ctx);
