#include "MyModule.h"

MyModule::MyModule(xewe::Os& host, MyModuleConfig config)
    : Module(host,                      // registers this module with the Os, right here
          /* id                  */ "my",          // command group ($my ...) and NVS namespace; <= 15 chars
          /* name                */ "My Module",
          /* description         */ "Remembers a number in NVS",
          /* requires_init_setup */ false,         // true would run begin_routines_init() once
          /* can_be_disabled     */ true,          // adds $my enable / $my disable
          /* has_cli_commands    */ true)          // adds $my status / $my reset, allows our own
    , config(config) {
    // a command of our own: $my bump, no arguments (the Cli checks the count)
    register_command({"bump", "Add one to the number", "$my bump", 0,
        [this](xewe::span<const std::string>) { set_number(number + 1); }});
}

xewe::Settings MyModule::settings() const {
    // one row per setting: key (= NVS key, <= 15 chars), min, max, default, doc; checked at build time
    static constexpr xewe::SettingDef table[] = {
        xewe::setting<&MyModule::number>("number", 0, 1000, 0, "The remembered number"),
        xewe::setting<&MyModule::beat_s>("beat_s", 1, 3600, 10, "Heartbeat period, s"),
        xewe::setting<&MyModule::label> ("label", 16, "my", "Heartbeat prefix"),
    };
    return {table, this};
}

void MyModule::loop() {
    if (!config.heartbeat || millis() - last_beat_ms < beat_s * 1000UL) return;   // non-blocking timer
    last_beat_ms = millis();
    os.serial.printf("%s: number is %u", label.c_str(), number);
}

void MyModule::set_number(uint16_t value, const void* origin) {
    if (is_disabled(true)) return;      // disabled modules stay callable; refuse politely
    number = value > 1000 ? 1000 : value;
    os.nvs.write<uint16_t>(id, "number", number);   // the row's NVS key: survives reboots
    listeners.notify([&](NumberListener& l) { l.on_number(number, origin); });
}

void MyModule::on_setting_changed(const xewe::SettingDef& def) {
    if (std::string_view(def.key) == "number") {
        listeners.notify([&](NumberListener& l) { l.on_number(number, nullptr); });   // from the CLI
    }
}
