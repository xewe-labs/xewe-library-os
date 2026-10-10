// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-core/src/XeWeCore/Utils/AsyncTimer.h
#pragma once

#include <Arduino.h>

#include <cinttypes>
#include <type_traits>

#include "Debug.h"

#ifndef DEBUG_AsyncTimer
#define DEBUG_AsyncTimer 0
#endif

namespace xewe {

template <typename T>
class AsyncTimer {
    static_assert(std::is_arithmetic_v<T>,
        "AsyncTimer<T> requires an arithmetic type");

private:
    mutable uint32_t          start_time       {};
    mutable uint32_t          last_calc_time   {};
    static constexpr uint32_t calc_interval_ms = 5;

    uint32_t                  delay_ms;
    T start_val,              target_val;

    mutable bool              done             {false}, initiated{false};
    mutable double            progress         {0.0};

    void                      calculate_progress() const {
        if (done || !initiated) return;

        uint32_t now = millis();
        if (now - last_calc_time < calc_interval_ms) return;
        last_calc_time   = now;

        uint32_t elapsed = now - start_time;
        progress         = (double)elapsed / (double)delay_ms;

        if (progress >= 1.0) {
            done     = true;
            progress = 1.0;
        }
    }

public:
    AsyncTimer(uint32_t delay, T start = T(), T target = T())
        : delay_ms(delay)
        , start_val(start)
        , target_val(target) {
        DBG_PRINTF(AsyncTimer, "[AsyncTimer] Created: Delay=%" PRIu32 ", Start=%f, Target=%f\n",
            delay, (double)start, (double)target
        );
    }

    // prints the timer's internal state (only with DEBUG_AsyncTimer)
    void debug_dump(const char* label = "DUMP") const {
        DBG_PRINTF(AsyncTimer, "[AsyncTimer:%s] init:%s | done:%s | prog:%.2f | start_v:%f | target_v:%f | delay:%" PRIu32 " | start_t:%" PRIu32 "\n",
            label,
            initiated ? "YES" : "NO",
            done ? "YES" : "NO",
            progress,
            (double)start_val,
            (double)target_val,
            delay_ms,
            start_time
        );
    }

    void initiate() {
        start_time     = millis();
        // Ensure the first calculation isn't throttled
        last_calc_time = (start_time > calc_interval_ms) ? (start_time - calc_interval_ms) : 0;

        progress       = 0.0;
        done           = false;
        initiated      = true;

        DBG_PRINTLN(AsyncTimer, "-> AsyncTimer::initiate()");
        debug_dump("INITIATED");
    }

    void reset() {
        done           = false;
        progress       = 0.0;
        initiated      = false;
        start_time     = 0;
        last_calc_time = 0;

        DBG_PRINTLN(AsyncTimer, "-> AsyncTimer::reset()");
        debug_dump("RESET_STATE");
    }

    void reset(T new_start, T new_target) {
        start_val  = new_start;
        target_val = new_target;
        reset();
    }

    void reset(uint32_t new_delay, T new_start, T new_target) {
        delay_ms   = new_delay;
        start_val  = new_start;
        target_val = new_target;
        reset();
    }

    T get_current_value() const {
        calculate_progress();
        return done ? target_val : T(start_val + (target_val - start_val) * progress);
    }

    T        get_start_value() const { return start_val; }
    T        get_target_value() const { return target_val; }
    double   get_progress() const { return progress; }

    uint32_t                  get_delay_ms     () const { return delay_ms; };

    bool     is_done() const {
        calculate_progress();
        return done;
    }

    bool is_not_done() const { return !is_done(); }
    bool is_active() const { return initiated; }

    void terminate() {
        initiated = false;
        DBG_PRINTLN(AsyncTimer, "-> AsyncTimer::terminated early.");
    }
};

} // namespace xewe
