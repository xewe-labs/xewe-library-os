# XeWeOS — a modular firmware base for ESP32

Personal project (xewe-labs) · 2026-09-10 → 2026-09-20 · Solo: Max Dokukin · Status: Released (1.0.0)

> Full reference: [`doc/`](doc/) · Agent rules: [`doc/AGENTS.md`](doc/AGENTS.md)

## Overview

A modular firmware base for ESP32 (C3, C6, S3). XeWeOS holds only the core; every other feature is a separate library you
plug in by declaring it in your sketch. The core is three classes in `namespace xewe::os`: a `ModuleController` that owns
the shared services (serial console, NVS storage, command line, system), a `Module` base class with a fixed boot lifecycle,
per-module persistence and a `$<id>` command group, and the built-in `System` module. Firmware is assembled in the `.ino`:
modules register themselves when they are constructed, so declaration order is begin order and loop order. The library was
extracted from the monolithic [XeWe OS](https://github.com/xewe-labs/xewe-os) firmware in September 2026 and is the top of
the XeWe Arduino library family ([XeWeUtils](https://github.com/xewe-labs/xewe-library-utils) →
[XeWeSerial](https://github.com/xewe-labs/xewe-library-serial) → [XeWeCli](https://github.com/xewe-labs/xewe-library-cli) →
[XeWeNvs](https://github.com/xewe-labs/xewe-library-nvs)).

## Highlights

- The framework knows no concrete modules: 893 lines of C++ in 7 files under `src/`, three classes, one entry header `XeWeOS.h`
- Every module gets, for free, a first-boot "enable this module?" question, a one-time init setup, an NVS namespace named after its
  id and a `$<id>` command group with `status` / `reset` (plus `enable` / `disable` when it can be disabled) — `src/Module/`
- Dependencies between modules are constructor references plus `add_requirement()`; disabling a module cascades to its dependents
- Built-in `$system` group with 8 commands: `restart`, `reboot`, `info`, `set_device_name`, `mac`, `uid`, `status` (a table of
  every module) and `reset` (a confirmed factory reset) — `src/System/`
- Exhaustive reference in [`doc/`](doc/) (5 pages, 703 lines), three example sketches of increasing scope and a copy-paste
  [`extras/ModuleTemplate`](extras/ModuleTemplate) with every hook commented
- Released 0.1.0 (2026-09-14) and 1.0.0 (2026-09-17) in lockstep with the four XeWe libraries it depends on

## How it works

```
sketch (.ino): ModuleController os({...}); ModuleA a(os); ModuleB b(os, a);   ← constructors self-register, in declaration order
setup(): os.begin() → serial.begin → NVS errors routed to the console → banner → module.begin() × N → (first boot: reboot)
loop():  os.loop()  → xewe_cli.loop() (runs complete $group command lines) → module.loop() for every ENABLED module
```

- **`ModuleController`** (`src/ModuleController/`) — owns the core services and the list of registered modules; `begin()`,
  `loop()`, `register_module`, `get_module(id)`, `get_modules()`, `get_config()`. Non-copyable.
- **`Module`** (`src/Module/`) — base class: constructor flags, the lifecycle hooks, `add_requirement`, `enable` / `disable` /
  `reset`, `status`, `register_command`, `run_with_dots`. Neither copyable nor movable (the controller holds raw pointers).
- **`System`** (`src/System/`) — the built-in module: a member of the controller, so it always registers and begins first;
  cannot be disabled.

### Core

`xewe::os::ModuleController` owns four services, available to every module:

| Member | Type | Library |
|---|---|---|
| `os.serial` | `xewe::SerialPort` | [XeWeSerial](https://github.com/xewe-labs/xewe-library-serial) |
| `os.nvs` | `xewe::Nvs` | [XeWeNvs](https://github.com/xewe-labs/xewe-library-nvs) |
| `os.xewe_cli` | `xewe::Cli` | [XeWeCli](https://github.com/xewe-labs/xewe-library-cli) |
| `os.system` | `xewe::os::System` (a Module: `$system ...`) | this library |

### Assembling firmware

Assembly happens in the `.ino`. Modules register themselves when constructed and
begin in declaration order, so declare the controller first, then dependencies
before the modules that use them.

```cpp
#include <XeWeOS.h>
#include "BlinkModule.h"

xewe::os::ModuleController os({
    .project_name    = "blink-device",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
});

BlinkModule blink(os, {.pin = 8});

void setup() { os.begin(); }
void loop()  { os.loop();  }
```

`ModuleControllerConfig` also takes `url`, `serial` (a `xewe::SerialPortConfig`) and
`print_banner`. The `url` defaults to this library's repository — set it to your own project or clear it.

### Writing a module

Derive from `xewe::os::Module`, take settings in the constructor, and override only
what you need. Start from [`extras/ModuleTemplate`](extras/ModuleTemplate) (every hook,
commented) or see `examples/02_CustomModule/BlinkModule.h` for a complete one.

```cpp
class BlinkModule : public xewe::os::Module {
public:
    BlinkModule(xewe::os::ModuleController& os, BlinkConfig config = {})
        : Module(os, "blink", "Blink", "Blinks an LED",
                 /* requires_init_setup */ true,
                 /* can_be_disabled     */ true,
                 /* has_cli_commands    */ true)
        , config(config) {
        register_command({"period", "Set period", "$blink period 250", 1,
                          [this](xewe::span<const std::string> args) { /* ... */ }});
    }
    void begin_routines_common() override { /* ... */ }
    void loop() override { /* ... */ }
};
```

Rules of thumb:

* **The framework knows no concrete modules.** A firmware project keeps its own modules in
  `src/<Name>/<Name>.{h,cpp}` and assembles them in the `.ino`; reusable ones become their own
  libraries.
* **Dependencies are constructor references.** A module that uses another takes it by
  reference, stores it, and calls `add_requirement(other)`:

  ```cpp
  WebInterface(xewe::os::ModuleController& os, Wifi& wifi) : Module(os, ...), wifi(wifi) {
      add_requirement(wifi);
  }
  ```

  Declare `wifi` before `web_interface` in the sketch.
* **Use the core services through the controller:** `controller.serial` for output and
  prompts, `controller.nvs` with the module `id` as namespace, `controller.xewe_cli.execute(line)`
  to run commands (buttons, schedules, web requests), `controller.system`.
* **`loop()` must not block;** every module shares it.
* **Don't name objects `cli`** when constructing them with parentheses: the Arduino core defines a
  `cli()` macro. The core uses `xewe_cli`.
* **A module `id` is both the CLI group and the NVS namespace,** so it is capped at 15 characters.

### Lifecycle

`os.begin()` calls `begin()` on each module in order:

1. On first boot, a module with `can_be_disabled` asks whether to enable it.
2. If any requirement (`add_requirement(other)`) is disabled, the module is disabled too.
3. `begin_routines_required()` runs on every boot.
4. `begin_routines_init()` runs until it completes once (when `requires_init_setup`),
   otherwise `begin_routines_regular()` runs.
5. `begin_routines_common()` runs last.

A disabled module skips steps 3-5 but stays registered, and other modules may still
call it, so public functions of a module that can be disabled should start with
`if (is_disabled()) return;`. The full eight-step sequence is in [`doc/module.md`](doc/module.md#lifecycle).

Each module stores its state in the NVS namespace named after its `id`
(`is_enabled`, `not_first_boot`, `init_complete`, plus its own keys), so `$<id> reset`
wipes exactly that module.

**The first boot of a new device ends in an automatic reboot** (after every module has run its setup), so nothing after
`os.begin()` runs on that boot. **`disable()` implies `reset()` and cascades to every dependent module.**
More of these in [`doc/README.md`](doc/README.md#things-that-surprise-people).

### Commands

With `has_cli_commands`, a module gets a `$<id>` command group with `status` and
`reset` (plus `enable` / `disable` when it can be disabled). Add more with
`register_command`. The built-in `$system` group:

| Command | Effect |
|---|---|
| `$system restart` / `reboot` | restart after a 1000 ms delay |
| `$system info` | chip model, cores, revision, IDF version, flash size and speed, Wi-Fi MAC |
| `$system set_device_name "<name>"` | store the device name |
| `$system mac` | MAC of each interface that reads back (`wifi_sta`, `wifi_ap`, `bt`, `eth`) |
| `$system uid` | eFuse base MAC and `uid64`, the first 8 bytes of its SHA-256 |
| `$system status` | a table of every registered module |
| `$system reset` | factory reset, after a confirmation |

### Changes from the monolithic xewe-os

| Before | Now |
|---|---|
| Controller has a member for every module | Only core services; modules self-register from the sketch |
| `begin(const ModuleConfig&)` + `static_cast` | Config passed to the module constructor; `begin_routines_*()` take no arguments |
| `controller.serial_port`, `controller.command_executor` | `controller.serial`, `controller.xewe_cli` |
| `commands_storage.push_back(...)` | `register_command(...)` |
| `command_executor.parse(line)` | `xewe_cli.execute(line)` |
| `Config.h` macros (`BUILD_VERSION`, ...) | `ModuleControllerConfig` |
| Central `Debug.h` | `#ifndef DEBUG_<Class>` per library, enabled via build flags |
| `Nvs::reset` erases flash | `Nvs::erase_all()`; `$system reset` does a factory reset |
| Module `enable()` always restarted | Restarts only when `do_restart` is true |

## Results

| Item | Value | Note |
|---|---|---|
| Releases | 0.1.0 (2026-09-14), 1.0.0 (2026-09-17) | git tags + GitHub releases; lockstep with the four dependencies |
| Core source | 893 lines of C++, 7 files, 3 classes | `src/` |
| Reference docs | 5 pages, 703 lines | `doc/` |
| Examples | 3 sketches, 283 lines | `examples/`, plus `extras/ModuleTemplate` (102 lines) |
| Targets | ESP32-C3, C6, S3 | `architectures=esp32`; the dependencies below it also declare non-ESP32 cores |
| Commits | 11, 2026-09-10 → 2026-09-20 | `git log` |

There are no benchmarks: the measurable outcome is the shape of the API — a core that knows no concrete modules — and the
releases built on it.

## Getting started

Install **XeWeOS** from the Arduino Library Manager (it pulls XeWeUtils, XeWeSerial, XeWeNvs and XeWeCli, and ArduinoJson
through XeWeNvs), or with PlatformIO (`library.json`, platform `espressif32`). Then open
`File → Examples → XeWeOS → 01_Basic`.

For local development, clone the library repos next to this one:

```bash
cd ..   # the folder holding all xewe-labs repos
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc \
  --library xewe-library-utils --library xewe-library-serial --library xewe-library-nvs \
  --library xewe-library-cli --library xewe-library-os \
  xewe-library-os/examples/02_CustomModule
```

Requirements: the ESP32 Arduino core (the code uses ESP-IDF calls such as `esp_chip_info`, `esp_read_mac` and
`esp_log_level_set`). Open the serial monitor at 115200 baud (the XeWeSerial default); the first boot asks for a device
name and then reboots.

### Examples

`01_Basic`, `02_CustomModule` and `03_ModuleDependencies` in [`examples/`](examples/), in
increasing order of scope; the last one shows two modules where one requires the other.

| | | |
|---|---|---|
| low | [`01_Basic`](examples/01_Basic) | the minimum sketch: a controller, `begin()`, `loop()` |
| mid | [`02_CustomModule`](examples/02_CustomModule) | one module with config, a CLI command, init setup and a non-blocking loop |
| high | [`03_ModuleDependencies`](examples/03_ModuleDependencies) | two modules where one requires the other: the cascade, a `status()` override, and a `get_module` lookup |

### Dependencies

XeWeUtils, XeWeSerial, XeWeNvs, XeWeCli (and ArduinoJson through XeWeNvs), each `>=1.0.0` in `library.properties`.

## Documents

- [doc/README.md](doc/README.md) — index and "things that surprise people"
- [doc/controller.md](doc/controller.md) — `ModuleController`, its config, the boot sequence, declaration order
- [doc/module.md](doc/module.md) — `Module`: constructor flags, the eight-step lifecycle, requirements, enable/disable/reset, NVS keys
- [doc/system.md](doc/system.md) — `System`: every `$system` command, the boot header, the factory reset
- [doc/AGENTS.md](doc/AGENTS.md) — rules for coding agents working in this repository

### The XeWe library family

XeWeOS is the top of the family; the libraries below are listed in dependency order.

| Title | Portfolio page | GitHub repo |
|---|---|---|
| XeWeOS Framework (this library) | [maxdokukin.com/projects/xewe-library-os](https://maxdokukin.com/projects/xewe-library-os) | [xewe-labs/xewe-library-os](https://github.com/xewe-labs/xewe-library-os) |
| XeWeUtils | [maxdokukin.com/projects/xewe-library-utils](https://maxdokukin.com/projects/xewe-library-utils) | [xewe-labs/xewe-library-utils](https://github.com/xewe-labs/xewe-library-utils) |
| XeWeSerial | [maxdokukin.com/projects/xewe-library-serial](https://maxdokukin.com/projects/xewe-library-serial) | [xewe-labs/xewe-library-serial](https://github.com/xewe-labs/xewe-library-serial) |
| XeWeCli | [maxdokukin.com/projects/xewe-library-cli](https://maxdokukin.com/projects/xewe-library-cli) | [xewe-labs/xewe-library-cli](https://github.com/xewe-labs/xewe-library-cli) |
| XeWeNvs | [maxdokukin.com/projects/xewe-library-nvs](https://maxdokukin.com/projects/xewe-library-nvs) | [xewe-labs/xewe-library-nvs](https://github.com/xewe-labs/xewe-library-nvs) |

Built on it: the [XeWe OS](https://maxdokukin.com/projects/xewe-os) firmware
([xewe-labs/xewe-os](https://github.com/xewe-labs/xewe-os)).
