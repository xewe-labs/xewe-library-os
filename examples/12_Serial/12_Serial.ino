// Reference demo: SerialPort output, typed prompts and non-blocking line input.
// XeWeCore Serial: formatted output, typed input prompts, and reading lines
// without blocking.
#include <XeWeCore.h>

xewe::SerialPort serial;

void setup() {
    // Defaults are 115200 baud, 2048/1024 byte buffers, a 1000 ms startup delay
    // (USB CDC needs time to enumerate) and echo on. Override what you need:
    serial.begin({.baud_rate = 115200, .echo = true});

    serial.print_header("XeWeCore Serial Demo");   // +---+ | title | +---+

    // Wrapping and alignment only happen when message_width > 0.
    serial.print("this long sentence is wrapped and centred inside a box",
                 xewe::str::kCRLF, "|", 'c', 'w', 46, 1, 1);
    serial.print_separator();

    std::string name = serial.get_string("What is your name?", 1, 32);
    int         age  = serial.get_int("How old are you?", 0, 150);

    if (serial.get_yn("Print a summary?")) {
        std::string age_str = std::to_string(age);
        serial.print_table({
            {"Field", "Value"},
            {"Name",  name},
            {"Age",   age_str},
        }, "Summary");
    }

    serial.printf("up %lu ms", millis());       // printf appends CRLF for you
    serial.print("Now echoing lines you type:");
}

void loop() {
    serial.loop();   // non-blocking: drains the port and assembles lines

    if (serial.has_line()) {
        const std::string line = serial.read_line();
        serial.printf("you said: %s", line.c_str());
    }
}
