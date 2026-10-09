#include "MyModule.h"

MyModule::MyModule(xewe::Os& host, MyModuleConfig config)
    : Module(host,                      // registers this module with the Os, right here
          /* id                  */ "my",          // command group ($my ...) and NVS namespace; <= 15 chars
          /* name                */ "My Module",
          /* description         */ "Remembers one number in NVS",
          /* requires_init_setup */ false,         // true would run begin_routines_init() once
          /* can_be_disabled     */ true,          // adds $my enable / $my disable
          /* has_cli_commands    */ true)          // adds $my status / $my reset, allows our own
    , config(config) {
    // $my set <n>: one argument; the Cli checks the count and prints the usage line otherwise.
    register_command({"set", "Store a number (0-1000) in NVS", "$my set 42", 1,
        [this](xewe::span<const std::string> args) {
            // validate<T>(text, min, max) parses and range-checks; empty optional on bad input
            if (auto n = xewe::validate<uint16_t>(args[0], 0, 1000)) set_number(*n);
            else os.serial.print("Usage: $my set <0-1000>");
        }});

    // $my show: no arguments.
    register_command({"show", "Print the stored number", "$my show", 0,
        [this](xewe::span<const std::string>) {
            os.serial.printf("number = %u", number);
        }});
}

void MyModule::begin_routines_common() {
    // NVS namespace == module id; the third argument is the value when nothing is stored yet
    number = os.nvs.read<uint16_t>(id, "number", 0);
}

void MyModule::loop() {
    if (!config.heartbeat || millis() - last_beat_ms < 10000) return;   // non-blocking timer
    last_beat_ms = millis();
    os.serial.printf("my: number is %u", number);
}

std::string MyModule::status(const bool verbose) const {
    // compose, don't replace: keep the base line (enabled, ...) and add our own state
    std::string s = Module::status(false) + ", number " + std::to_string(number);
    if (verbose) os.serial.print(s);
    return s;
}

void MyModule::set_number(uint16_t value) {
    if (is_disabled(true)) return;      // disabled modules stay callable; refuse politely
    number = value;
    os.nvs.write<uint16_t>(id, "number", number);   // survives reboots until $my disable/reset
    os.serial.printf("number = %u (saved)", number);
}
