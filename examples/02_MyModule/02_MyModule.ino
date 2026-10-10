// Level 2: one module of your own, living next to this sketch in MyModule.h/.cpp.
// Serial Monitor at 115200 baud. First boot asks for a device name and whether to
// enable "My Module". Then try:
//   $help my                  <- set, get, schema (from the settings table), bump, status, ...
//   $my set number 42         <- validated (0-1000), saved to NVS, the sketch's listener prints it
//   $my set number 5000       <- rejected: "! $my set number: expected u16 in [0, 1000]"
//   $my set beat_s 3          <- the heartbeat now every 3 s
//   $my get number            <- number=42, also after $system restart
//   $my schema                <- the table as JSON lines; $system schema: every module
//   $my bump                  <- our own command: number + 1
//   $system status            <- your module has a row in the table now
//   $my disable               <- asks "OK?" (two tries, 15 s each; no clear yes = cancelled).
//                                On yes: wipes the module's NVS namespace (back to the defaults),
//                                marks it disabled and restarts. While disabled its loop()
//                                does not run. `$my enable` brings it back.
//
// Read MyModule.h first (the contract), then MyModule.cpp (the table and the commands).
#include <XeWeCore.h>

#include "MyModule.h"

// Declaration order is registration order: the Os first, then the modules.
XeWeOs   os({.project_name = "my-module", .version = "0.1.0"});
MyModule my(os, {.heartbeat = true});   // set false to silence the heartbeat

// A listener: told about every change of the number.
struct NumberPrinter : NumberListener {
    void on_number(uint16_t value, const void* origin) override {
        if (origin == this) return;     // a change we made ourselves: no echo
        os.serial.printf("sketch: number changed to %u", value);
    }
} printer;

void setup() {
    os.begin();                 // begins every module: System first, then MyModule
    my.listeners.add(&printer);
}

void loop() {
    os.loop();                  // console, then MyModule::loop() while it is enabled
}
