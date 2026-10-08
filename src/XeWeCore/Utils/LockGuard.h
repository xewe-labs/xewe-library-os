// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/LockGuard.h
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


namespace xewe {

// RAII guard for a FreeRTOS mutex created with xSemaphoreCreateMutex().
// Not recursive: taking the same mutex twice from one task deadlocks.
class LockGuard {
public:
    explicit          LockGuard  (SemaphoreHandle_t mutex_handle)
        : mutex(mutex_handle) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }

                      ~LockGuard () { xSemaphoreGive(mutex); }

                      LockGuard  (const LockGuard&)            = delete;
    LockGuard&        operator=  (const LockGuard&)            = delete;

private:
    SemaphoreHandle_t mutex;
};

} // namespace xewe
