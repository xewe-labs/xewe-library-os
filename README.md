# XeWeCore

Modular firmware core for ESP32, as one Arduino library: a serial console, a `$group command`
command line, typed NVS storage, small helpers and a module framework. You write modules; the core
gives each one its commands, its stored settings and its place in the boot order.

* **Boards:** ESP32 family (arduino-esp32 3.x). The examples compile for ESP32-C3, C6 and S3.
* **Depends on:** [ArduinoJson](https://arduinojson.org/) 7, installed with it.

## Install

Arduino IDE: **Library Manager → search `XeWeCore` → Install**. With arduino-cli:
`arduino-cli lib install XeWeCore`.

```cpp
#include <XeWeCore.h>

XeWeOs os({.project_name = "my-device", .version = "0.1.0"});

void setup() { os.begin(); }
void loop()  { os.loop();  }
```

Upload, open the Serial Monitor at 115200 baud and type `$help`.

## Three levels

| Level | You need | Start with |
|---|---|---|
| 1. Use the console | Arduino IDE + XeWeCore | [`examples/01_Hello`](examples/01_Hello/01_Hello.ino): `$help`, `$system status`, a device name kept in NVS |
| 2. Write a module | the same, plus one module in your sketch folder | [`examples/02_MyModule`](examples/02_MyModule/02_MyModule.ino): your own `$my …` commands, a settings table, enable/disable |
| 3. Build a product | the [xewe-os](https://github.com/xewe-labs/xewe-os) project template and its tools | ready-made modules (Wi-Fi, web interface, scheduler, …), multi-chip builds, board tests, releases |

Levels 1 and 2 need nothing but the IDE. The other examples (`11_Utils` … `15_Os`) show one part
each; see [doc/examples.md](doc/examples.md).

## Documentation

[`doc/`](doc/README.md) explains what the core is and why it is built this way, then documents
every public type, command and stored key. Building and testing the library itself:
[doc/tests.md](doc/tests.md).

## License

GPL-3.0-only. See [LICENSE.txt](LICENSE.txt).
