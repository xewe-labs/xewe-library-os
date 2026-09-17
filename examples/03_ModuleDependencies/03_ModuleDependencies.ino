// XeWe OS (high): two modules, where one requires the other.
// Try: $help
//      $system status        <- a table of every registered module
//      $sensor read
//      $sensor limit 600
//      $alert disable        <- only alert stops
//      $sensor disable       <- CASCADES: alert is disabled and reset too
//
// Disabling a module resets it, which wipes its NVS namespace, and the cascade
// wipes every dependent's namespace as well. The confirmation prompt appears
// once, for the module you named — not for the dependents it takes with it.
#include <XeWeOS.h>

#include "SensorModule.h"
#include "AlertModule.h"

#define LED_PIN 8   // onboard on most C3/C6/S3 dev boards; change for yours

// Declaration order is registration order is begin() order is loop() order.
// The controller comes first; a module comes after everything it depends on.
xewe::os::ModuleController os({
    .project_name    = "sensor-alert",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
    .url             = "",            // clear the default, which points at the library repo
});

SensorModule sensor(os, {.pin = 1, .default_limit = 500});
AlertModule  alert(os, sensor, LED_PIN);   // after `sensor`: it holds a reference to it

void setup() {
    os.begin();

    // Modules can also be looked up by id. Prefer a constructor reference plus
    // add_requirement() for a real dependency; this is for the loose cases.
    if (xewe::os::Module* m = os.get_module("sensor")) {
        os.serial.printf("found module '%s' (%s)",
                         std::string(m->get_name()).c_str(),
                         m->is_enabled() ? "enabled" : "disabled");
    }

    // On the very first boot of a device os.begin() ends in a reboot, so
    // nothing after this line runs on that boot.
}

void loop() {
    os.loop();   // CLI first, then each ENABLED module's loop()
}
