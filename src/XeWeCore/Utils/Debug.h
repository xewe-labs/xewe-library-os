// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/Debug.h
#pragma once

#include <Arduino.h>

#include <cstdio>

// Debug output is opt-in per class. Each library defines its own flag with a
// default of 0 next to its code:
//
//     #ifndef DEBUG_MyClass
//     #define DEBUG_MyClass 0
//     #endif
//
// Enable one from the build instead of editing the library, e.g.
//     arduino-cli compile --build-property "compiler.cpp.extra_flags=-DDEBUG_MyClass=1"
// or in PlatformIO: build_flags = -DDEBUG_MyClass=1

#define DBG_ENABLED(cls)       (DEBUG_##cls)

#define DBG_PRINTLN(cls, msg)                           \
    do {                                                \
        if (DBG_ENABLED(cls)) {                         \
            Serial.print("[DBG] [");                    \
            Serial.print(#cls);                         \
            Serial.print("]: ");                        \
            Serial.println(msg);                        \
        }                                               \
    } while (0)

// Formats into a fixed stack buffer rather than calling Serial.printf, which the
// AVR/SAMD/STM32 Print class does not provide. Output longer than the buffer is
// truncated.
#ifndef DBG_PRINTF_BUFFER_SIZE
#define DBG_PRINTF_BUFFER_SIZE 128
#endif

#define DBG_PRINTF(cls, fmt, ...)                       \
    do {                                                \
        if (DBG_ENABLED(cls)) {                         \
            char _dbg_buf[DBG_PRINTF_BUFFER_SIZE];      \
            snprintf(_dbg_buf, sizeof(_dbg_buf),        \
                     (fmt), ##__VA_ARGS__);             \
            Serial.print("[DBG] [");                    \
            Serial.print(#cls);                         \
            Serial.print("]: ");                        \
            Serial.print(_dbg_buf);                     \
        }                                               \
    } while (0)
