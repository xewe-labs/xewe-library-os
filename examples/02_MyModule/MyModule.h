// MyModule: the smallest complete module. Copy MyModule.h/.cpp into your own sketch folder,
// rename MyModule -> YourName and "my" -> your command group, and keep what you need.
#pragma once

#include <XeWeCore.h>

// Settings fixed at compile time, passed from the sketch: MyModule my(os, {.heartbeat = false});
struct MyModuleConfig {
    bool heartbeat = true;      // the bool setting: print the number every 10 s from loop()
};

class MyModule : public xewe::Module {
public:
    // Name the Os parameter `host`, never `os`: `os` is the protected member that the
    // method bodies and the [this] command lambdas use.
    explicit MyModule(xewe::Os& host, MyModuleConfig config = {});

    // begin: the Os calls the begin_routines_* hooks from os.begin(), in declaration order.
    //   begin_routines_required()  every boot, first
    //   begin_routines_init()      first boot only, until it completes (needs requires_init_setup)
    //   begin_routines_regular()   every boot after init has completed
    //   begin_routines_common()    every boot, last   <- the only one this module needs
    void        begin_routines_common()                     override;

    // loop: called from os.loop() while the module is enabled. Must never block.
    void        loop()                                      override;

    // status: one line used by `$my status` and by the `$system status` table.
    std::string status(const bool verbose = false)    const override;

    // Public API: other modules (or the sketch) may call this too.
    void        set_number(uint16_t value);

private:
    MyModuleConfig config;
    uint16_t       number       = 0;    // the NVS setting, key "number" in namespace "my"
    uint32_t       last_beat_ms = 0;
};
