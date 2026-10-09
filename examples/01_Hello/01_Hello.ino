// Level 1: XeWe OS with nothing of your own yet. Upload, open the Serial Monitor
// at 115200 baud (line ending: Newline or Both NL & CR) and you get:
//   - a command console: every command starts with `$`, `$help` lists them all
//   - a first-boot setup: the device asks for a name, saves it and reboots once
//   - settings kept in NVS (flash), so the name survives power cycles
//   - the built-in `system` module: status, chip/build info, restart
//
// Type these first:
//   $help                                   <- every command group and command
//   $system status                          <- one line per registered module
//   $system info                            <- chip model, IDF version, flash size, MAC
//   $system set_device_name "Desk Lamp"     <- rename; stored in NVS
//   $system restart                         <- reboot; the name is still there
//
// Next step: 02_MyModule adds a module of your own with its own commands.
#include <XeWeCore.h>

XeWeOs os({.project_name = "hello", .version = "0.1.0"});

void setup() {
    os.begin();   // console, NVS, built-in commands; prompts for a name on first boot
}

void loop() {
    os.loop();    // reads the console and runs commands; must be called often
}
