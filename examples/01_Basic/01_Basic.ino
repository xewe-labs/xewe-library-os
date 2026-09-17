// XeWe OS (low): the smallest assembly. Core services only (serial, nvs, xewe_cli, system).
// Try: $help   $system status   $system info
#include <XeWeOS.h>

xewe::os::ModuleController os({
    .project_name    = "basic-device",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
});

void setup() {
    os.begin();
}

void loop() {
    os.loop();
}
