// MyModule: the smallest complete module. Copy MyModule.h/.cpp into your own sketch folder,
// rename MyModule -> YourName and "my" -> your command group, and keep what you need.
#pragma once

#include <XeWeCore.h>

// Settings fixed at compile time, passed from the sketch: MyModule my(os, {.heartbeat = false});
struct MyModuleConfig {
    bool heartbeat = true;      // print the number every beat_s seconds from loop()
};

// Who wants to hear about changes implements this (the sketch does). `origin` is whoever made the
// change (nullptr from the CLI): a listener that also sets passes `this` and skips its own echo.
struct NumberListener {
    virtual void on_number(uint16_t value, const void* origin) = 0;
};

class MyModule : public xewe::Module {
public:
    // Name the Os parameter `host`, never `os`: `os` is the protected member that the
    // method bodies and the [this] command lambdas use.
    explicit MyModule(xewe::Os& host, MyModuleConfig config = {});

    // Run-time settings: the table in MyModule.cpp. The core loads them at begin (default, then
    // NVS) and adds `$my set|get|schema`, the status lines and the rows in `$system schema`.
    xewe::Settings settings()                               const override;

    // loop: called from os.loop() while the module is enabled. Must never block.
    void        loop()                                      override;

    // Public API: other modules (or the sketch) may call this too.
    void        set_number(uint16_t value, const void* origin = nullptr);

    xewe::ListenerSet<NumberListener> listeners;    // up to 4, no heap: listeners.add(&l)

protected:
    // `$my set <key> <value>` applied and saved a row: tell the listeners
    void        on_setting_changed(const xewe::SettingDef& def) override;

private:
    MyModuleConfig config;
    uint16_t       number       = 0;    // the rows' members; NVS keys "number", "beat_s", "label"
    uint16_t       beat_s       = 10;
    std::string    label;
    uint32_t       last_beat_ms = 0;
};
