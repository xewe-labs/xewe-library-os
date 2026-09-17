// A plug-in module: config via constructor, NVS-backed setting, first-boot setup, CLI commands.
#pragma once

#include <XeWeOS.h>


struct BlinkConfig {
    uint8_t  pin               = 8;
    uint32_t default_period_ms = 500;
};

class BlinkModule : public xewe::os::Module {
public:
    BlinkModule(xewe::os::ModuleController& controller, BlinkConfig config = {})
        : Module(controller,
              /* id                  */ "blink",
              /* name                */ "Blink",
              /* description         */ "Blinks an LED at a configurable period",
              /* requires_init_setup */ true,
              /* can_be_disabled     */ true,
              /* has_cli_commands    */ true)
        , config(config) {
        register_command({
            "period",
            "Set blink period in ms (50-10000)",
            "$blink period 250",
            1,
            [this](std::span<const std::string> args) {
                auto value = xewe::validate<uint32_t>(args[0], 50, 10000);
                if (!value) {
                    this->controller.serial.print("Error: period must be 50-10000");
                    return;
                }
                set_period(*value);
            }
        });
    }

    // runs once, on the first boot after the module is enabled
    void begin_routines_init() override {
        uint32_t period = controller.serial.get_uint32("Blink period in ms?", 50, 10000, 0, 0, config.default_period_ms);
        controller.nvs.write<uint32_t>(id, "period", period);
    }

    // runs on every boot while enabled
    void begin_routines_common() override {
        pinMode(config.pin, OUTPUT);
        period_ms = controller.nvs.read<uint32_t>(id, "period", config.default_period_ms);
    }

    void loop() override {
        if (millis() - last_toggle_ms < period_ms) return;
        last_toggle_ms = millis();
        digitalWrite(config.pin, !digitalRead(config.pin));
    }

    std::string status(const bool verbose = false) const override {
        std::string s = Module::status(false) + ", period " + std::to_string(period_ms) + " ms";
        if (verbose) controller.serial.print(s);
        return s;
    }

    void set_period(uint32_t ms) {
        if (is_disabled(true)) return;
        period_ms = ms;
        controller.nvs.write<uint32_t>(id, "period", ms);
        controller.serial.printf("Blink period set to %u ms", ms);
    }

private:
    BlinkConfig config;
    uint32_t    period_ms      = 500;
    uint32_t    last_toggle_ms = 0;
};
