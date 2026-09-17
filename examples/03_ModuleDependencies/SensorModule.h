// A module with its own settings, a first-boot setup and a CLI command.
#pragma once

#include <XeWeOS.h>

struct SensorConfig {
    uint8_t  pin            = 1;
    uint16_t default_limit  = 500;
};

class SensorModule : public xewe::os::Module {
public:
    SensorModule(xewe::os::ModuleController& os, SensorConfig config = {})
        : Module(os, "sensor", "Sensor", "Reads a value and compares it to a limit",
                 /* requires_init_setup */ true,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , config(config) {
        register_command({"limit", "Set the alert limit", "$sensor limit 600", 1,
            [this](std::span<const std::string> args) {
                if (auto v = xewe::validate<uint16_t>(args[0], 0, 4095)) {
                    set_limit(*v);
                } else {
                    controller.serial.print("Usage: $sensor limit <0-4095>");
                }
            }});

        register_command({"read", "Print the current value", "$sensor read", 0,
            [this](std::span<const std::string>) {
                controller.serial.printf("value %u (limit %u)", read_value(), limit);
            }});
    }

    // Runs once, until it completes: the first-boot question for this module.
    void begin_routines_init() override {
        limit = controller.serial.get_uint16("Alert limit (0-4095)?",
                                             0, 4095, 0, 0, config.default_limit);
        controller.nvs.write<uint16_t>(id, "limit", limit);
    }

    // Runs on every boot, last. NVS namespace == this module's id.
    void begin_routines_common() override {
        pinMode(config.pin, INPUT);
        limit = controller.nvs.read<uint16_t>(id, "limit", config.default_limit);
    }

    uint16_t read_value() const { return analogRead(config.pin); }

    bool over_limit() const { return read_value() > limit; }

    void set_limit(uint16_t value) {
        if (is_disabled()) return;   // a disabled module stays registered and callable
        limit = value;
        controller.nvs.write<uint16_t>(id, "limit", limit);
        controller.serial.printf("limit %u", limit);
    }

    // Compose, don't replace: keep the base line and add our own state.
    std::string status(const bool verbose = false) const override {
        std::string s = Module::status(false) + ", limit " + std::to_string(limit);
        if (verbose) controller.serial.print(s);
        return s;
    }

private:
    SensorConfig config;
    uint16_t     limit = 0;
};
