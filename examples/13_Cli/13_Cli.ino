// Reference demo: a standalone Cli with groups, quoting and argument checks.
// XeWeCore Cli: several groups, quoted arguments, argument checking, and
// running a command from code.
// Try: $help
//      $dev name "Kitchen Lights"
//      $dev name "say \"hi\""
//      $led set 300          <- rejected by validate, with a usage line
//      $led set              <- argument count mismatch, prints sample_usage
//      $dev                  <- same as $help dev
//
// The object is named xewe_cli, not cli: the ESP32 core defines a function-like
// `cli` macro, so a sketch-level `xewe::Cli cli(serial);` does not compile.
#include <XeWeCore.h>

#define LED_PIN 8   // onboard LED on many dev boards; change for yours

xewe::SerialPort serial;
xewe::Cli        xewe_cli(serial);

std::string device_name = "unnamed";

void setup() {
    serial.begin();
    pinMode(LED_PIN, OUTPUT);

    // add_group returns a reference, and re-adding an id keeps its commands.
    xewe_cli.add_group("led", "LED");
    xewe_cli.add_group("dev", "Device");

    // add_command returns false for an unknown group, an empty name or an
    // empty function; worth checking while you are wiring things up.
    const bool added = xewe_cli.add_command("led", {"set", "Set level 0-255", "$led set 128", 1,
        [](xewe::span<const std::string> args) {
            // The argument COUNT is checked before we get here; the VALUE is ours.
            if (auto level = xewe::validate<uint8_t>(args[0], 0, 255)) {
                analogWrite(LED_PIN, *level);
                serial.printf("level %u", *level);
            } else {
                serial.print("Usage: $led set <0-255>");
            }
        }});
    if (!added) serial.print("could not register $led set");

    xewe_cli.add_command("dev", {"name", "Set the device name", "$dev name \"Kitchen\"", 1,
        [](xewe::span<const std::string> args) {
            device_name = args[0];   // COPY: the span does not outlive this call
            serial.printf("name: %s", device_name.c_str());
        }});

    xewe_cli.add_command("dev", {"show", "Print the device name", "$dev show", 0,
        [](xewe::span<const std::string>) {
            serial.printf("name: %s", device_name.c_str());
        }});

    // Introspection: what got registered?
    for (const auto& [id, group] : xewe_cli.get_groups()) {     // alphabetical by id
        serial.printf("group %-7s %u command(s)", id.c_str(),
                      static_cast<unsigned>(group.commands.size()));
    }

    // Commands can be run from code with the same parser the console uses.
    xewe_cli.execute("$dev name \"Kitchen Lights\"");

    xewe_cli.print_help("dev");    // print one group's table from code
    serial.print("Type $help");
}

void loop() {
    xewe_cli.loop();
}
