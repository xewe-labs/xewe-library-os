# XeWeCore

Modular firmware core for ESP32: a serial console, a command line, typed NVS storage, small
helpers and a module framework, in one Arduino library.

XeWeCore 2.0.0 replaces XeWeUtils, XeWeSerial, XeWeCli, XeWeNvs and XeWeOS (all 1.0.0). Behaviour,
NVS keys, module ids and CLI commands are unchanged (one exception, see
[Upgrading](#upgrading-from-the-100-libraries)), so devices keep their stored data.

* **Boards:** ESP32 family (arduino-esp32 3.x). The examples compile for ESP32-C3, C6 and S3.
* **Depends on:** [ArduinoJson](https://arduinojson.org/) 7.

## Use it

```cpp
#include <XeWeCore.h>

XeWeOs os({.project_name = "my-device", .version = "0.1.0"});

void setup() { os.begin(); }
void loop()  { os.loop();  }
```

Always include `<XeWeCore.h>`. The parts work on their own too: `xewe::SerialPort`, `xewe::Cli`,
`xewe::Nvs`, `xewe::FlexData` and the `xewe::str` helpers need no `XeWeOs`.

| Member | Type | What it is |
|---|---|---|
| `os.serial` | `xewe::SerialPort` | formatted console output and typed prompts |
| `os.nvs` | `xewe::Nvs` | typed key-value storage in flash |
| `os.cli` | `xewe::Cli` | `$group command args` parser and `$help` |
| `os.system` | `xewe::System` | the built-in module: restart, info, device name |

Modules derive from `xewe::Module`; start from [`extras/ModuleTemplate`](extras/ModuleTemplate).

* **Input queue:** serial input keeps up to four completed lines, and a line over 254 characters is dropped whole.
* **Bounded confirmations:** `$<module> disable` and `$system reset` ask at most twice, 15 s each, and cancel unless the answer is yes.
* **Test hooks:** building with `XEWE_TESTING` adds a `$test` command group for the hardware tests and costs nothing otherwise.

## Examples

`01_Utils`, `02_Serial`, `03_Cli`, `04_Nvs`, `05_Os` in [`examples/`](examples/).

## Upgrading from the 1.0.0 libraries

| 1.0.0 | 2.0.0 |
|---|---|
| `#include <XeWeOS.h>` (and the other four) | `#include <XeWeCore.h>` |
| `xewe::os::ModuleController`, `ModuleControllerConfig` | `xewe::Os` (alias `XeWeOs`), `xewe::OsConfig` |
| `xewe::os::Module`, `xewe::os::System` | `xewe::Module`, `xewe::System` |
| `controller.xewe_cli` | `os.cli` |
| protected `Module::controller` | `Module::os`; name the constructor's Os parameter `host` so `[this]` handlers use `os` directly (a parameter named `os` would hide the member) |
| global `AsyncTimer<T>` | `xewe::AsyncTimer<T>` |
| `depends_libraries=XeWeOS (>=0.1.0)` | `XeWeCore (>=2.0.0)` |

Behaviour is unchanged except that the default `OsConfig::url` printed in the boot header now
points to `https://github.com/xewe-labs/xewe-os-core`.

## Documentation

The full reference is in [`doc/`](doc/README.md). Rules for coding agents: [`doc/AGENTS.md`](doc/AGENTS.md).

## License

GPL-3.0-only. See [LICENSE.txt](LICENSE.txt).
