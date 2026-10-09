// Level 2: one module of your own, living next to this sketch in MyModule.h/.cpp.
// Serial Monitor at 115200 baud. First boot asks for a device name and whether to
// enable "My Module". Then try:
//   $help my             <- the module's commands: set, show, status, reset, enable, disable
//   $my set 42           <- validated (0-1000) and written to NVS
//   $my set 5000         <- rejected with a usage line
//   $my show             <- prints 42, also after $system restart
//   $system status       <- your module has a row in the table now
//   $my disable          <- asks "OK?" (two tries, 15 s each; no clear yes = cancelled).
//                           On yes: wipes the module's NVS namespace (the 42 is gone),
//                           marks it disabled and restarts. While disabled its loop()
//                           does not run and `$my set` refuses. `$my enable` brings it back.
//
// Read MyModule.h first (the contract), then MyModule.cpp (the commands).
#include <XeWeCore.h>

#include "MyModule.h"

// Declaration order is registration order: the Os first, then the modules.
XeWeOs   os({.project_name = "my-module", .version = "0.1.0"});
MyModule my(os, {.heartbeat = true});   // set false to silence the 10 s heartbeat

void setup() {
    os.begin();   // begins every module: System first, then MyModule
}

void loop() {
    os.loop();    // console, then MyModule::loop() while it is enabled
}
