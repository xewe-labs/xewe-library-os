# Examples

The sketches in [`examples/`](../examples) open from the Arduino IDE under
**File → Examples → XeWeCore**. `01` and `02` teach levels 1 and 2 (see
[Three levels](README.md#three-levels)); `11` to `15` show one part of the core each.

## Teaching sketches

- [`01_Hello`](../examples/01_Hello/01_Hello.ino): the Os alone. A serial console with `$help`,
  `$system …` and a device name kept in NVS.
- [`02_MyModule`](../examples/02_MyModule/02_MyModule.ino): a module of your own. It has a
  three-row settings table (`$my set|get|schema`, kept in NVS), a command of its own, a listener
  and enable/disable. Copy `MyModule.h` and `MyModule.cpp` to start a module.

## Reference demos

- [`11_Utils`](../examples/11_Utils/11_Utils.ino): string and validation helpers, AsyncTimer,
  Color, LockGuard.
- [`12_Serial`](../examples/12_Serial/12_Serial.ino): SerialPort output, typed prompts,
  non-blocking line input.
- [`13_Cli`](../examples/13_Cli/13_Cli.ino): a standalone Cli with groups, quoting and argument
  checks.
- [`14_Nvs`](../examples/14_Nvs/14_Nvs.ino): typed NVS values and a FlexData struct across
  reboots.
- [`15_Os`](../examples/15_Os/15_Os.ino): two modules where one requires the other.

Every example compiles for ESP32-C3, C6 and S3. [tests.md](tests.md) shows how to compile them
all at once.
