// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/tests/unit/shim/freertos/FreeRTOS.h
#pragma once

// Host stand-in for the few FreeRTOS names LockGuard uses. XeWeCore/Utils.h
// includes LockGuard unconditionally (ESP32 always has FreeRTOS), so every host
// build that reaches Utils.h through Serial.h or Cli.h needs these to exist.
// Single-threaded: a "mutex" is a counter, enough to check take/give pairing.

#include <cstdint>

typedef uint32_t TickType_t;
typedef int      BaseType_t;
#define portMAX_DELAY  ((TickType_t)0xffffffffUL)
#define pdTRUE         ((BaseType_t)1)
#define pdFALSE        ((BaseType_t)0)
