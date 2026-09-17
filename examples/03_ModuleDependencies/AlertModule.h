// A module that depends on another one.
#pragma once

#include <XeWeOS.h>

#include "SensorModule.h"

class AlertModule : public xewe::os::Module {
public:
    AlertModule(xewe::os::ModuleController& os, SensorModule& sensor, uint8_t led_pin)
        : Module(os, "alert", "Alert", "Blinks when the sensor is over its limit",
                 /* requires_init_setup */ false,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , sensor(sensor)
        , led_pin(led_pin) {
        // Records the edge in BOTH directions: sensor becomes our requirement,
        // and we become one of its dependents. That back-edge is what makes
        // `$sensor disable` cascade down to this module.
        add_requirement(sensor);

        register_command({"test", "Blink once", "$alert test", 0,
            [this](std::span<const std::string>) { blink(); }});
    }

    void begin_routines_common() override {
        pinMode(led_pin, OUTPUT);
    }

    // Only called while this module is enabled — the controller skips loop()
    // for disabled modules. It must not block.
    void loop() override {
        if (millis() - last_check < 1000) return;
        last_check = millis();

        // Safe even if `sensor` were disabled: its public functions guard
        // themselves, and a disabled requirement would have disabled us too.
        const bool over = sensor.over_limit();
        digitalWrite(led_pin, over ? HIGH : LOW);
    }

    // Called from the $alert test command handler, where a short block is fine.
    // Do NOT call this from loop(): every module shares that one loop.
    void blink() {
        if (is_disabled()) return;   // a disabled module stays registered and callable
        digitalWrite(led_pin, HIGH);
        delay(120);
        digitalWrite(led_pin, LOW);
    }

private:
    SensorModule& sensor;
    uint8_t       led_pin;
    uint32_t      last_check = 0;
};
