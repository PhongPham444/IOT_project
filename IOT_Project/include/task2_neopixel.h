#pragma once
#include "system_context.h"

void TaskNeoPixel(void* pv);
void TaskNeoController(void* pv);

void neo_init(SystemContext* ctx);
void neo_safe_set(SystemContext* ctx, uint32_t color);
