// XeWe OS (mid): assembling core + a custom module.
// Modules begin in the order they are declared; declare the controller first.
// Try: $help   $blink period 100   $blink disable   $system status
#include <XeWeOS.h>
#include "BlinkModule.h"

xewe::os::ModuleController os({
    .project_name    = "blink-device",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
});

BlinkModule blink(os, {.pin = 8, .default_period_ms = 300});

void setup() {
    os.begin();
}

void loop() {
    os.loop();
}
