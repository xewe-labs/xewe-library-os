// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/extras/host/shim/freertos/semphr.h
#pragma once

#include "FreeRTOS.h"

struct HostSemaphore { int held = 0; int takes = 0; int gives = 0; };
typedef HostSemaphore* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex () { return new HostSemaphore{}; }
inline void              vSemaphoreDelete      (SemaphoreHandle_t m) { delete m; }
inline BaseType_t        xSemaphoreTake        (SemaphoreHandle_t m, TickType_t) { ++m->held; ++m->takes; return pdTRUE; }
inline BaseType_t        xSemaphoreGive        (SemaphoreHandle_t m) { --m->held; ++m->gives; return pdTRUE; }
