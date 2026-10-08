// A module with its own settings, a first-boot setup and a CLI command.
#pragma once

#include <XeWeCore.h>

struct SensorConfig {
    uint8_t  pin            = 1;
    uint16_t default_limit  = 500;
};

class SensorModule : public xewe::Module {
public:
    SensorModule(xewe::Os& host, SensorConfig config = {})
        : Module(host, "sensor", "Sensor", "Reads a value and compares it to a limit",
                 /* requires_init_setup */ true,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , config(config) {
        // The Os parameter is named `host` so it never hides the member `os`;
        // the handlers (which run later) capture [this] and use os directly.
        register_command({"limit", "Set the alert limit", "$sensor limit 600", 1,
            [this](xewe::span<const std::string> args) {
                if (auto v = xewe::validate<uint16_t>(args[0], 0, 4095)) {
                    set_limit(*v);
                } else {
                    os.serial.print("Usage: $sensor limit <0-4095>");
                }
            }});

        register_command({"read", "Print the current value", "$sensor read", 0,
            [this](xewe::span<const std::string>) {
                os.serial.printf("value %u (limit %u)", read_value(), limit);
            }});
    }

    // Runs once, until it completes: the first-boot question for this module.
    void begin_routines_init() override {
        limit = os.serial.get_uint16("Alert limit (0-4095)?",
                                     0, 4095, 0, 0, config.default_limit);
        os.nvs.write<uint16_t>(id, "limit", limit);
    }

    // Runs on every boot, last. NVS namespace == this module's id.
    void begin_routines_common() override {
        pinMode(config.pin, INPUT);
        limit = os.nvs.read<uint16_t>(id, "limit", config.default_limit);
    }

    uint16_t read_value() const { return analogRead(config.pin); }

    bool over_limit() const { return read_value() > limit; }

    void set_limit(uint16_t value) {
        if (is_disabled()) return;   // a disabled module stays registered and callable
        limit = value;
        os.nvs.write<uint16_t>(id, "limit", limit);
        os.serial.printf("limit %u", limit);
    }

    // Compose, don't replace: keep the base line and add our own state.
    std::string status(const bool verbose = false) const override {
        std::string s = Module::status(false) + ", limit " + std::to_string(limit);
        if (verbose) os.serial.print(s);
        return s;
    }

private:
    SensorConfig config;
    uint16_t     limit = 0;
};
