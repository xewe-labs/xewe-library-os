# XeWeOS

> Full reference: [`doc/`](doc/) · Agent rules: [`doc/AGENTS.md`](doc/AGENTS.md)

A modular firmware base for ESP32 (C3, C6, S3). XeWeOS holds only the core; every
other feature is a separate library you plug in by declaring it in your sketch.

## Core

`xewe::os::ModuleController` owns four services, available to every module:

| Member | Type | Library |
|---|---|---|
| `os.serial` | `xewe::SerialPort` | XeWeSerial |
| `os.nvs` | `xewe::Nvs` | XeWeNvs |
| `os.xewe_cli` | `xewe::Cli` | XeWeCli |
| `os.system` | `xewe::os::System` (a Module: `$system ...`) | this library |

## Assembling firmware

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
`print_banner`.

## Writing a module

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
                          [this](std::span<const std::string> args) { /* ... */ }});
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
`if (is_disabled()) return;`.

Each module stores its state in the NVS namespace named after its `id`
(`is_enabled`, `not_first_boot`, `init_complete`, plus its own keys), so `$<id> reset`
wipes exactly that module.

### Commands

With `has_cli_commands`, a module gets a `$<id>` command group with `status` and
`reset` (plus `enable` / `disable` when it can be disabled). Add more with
`register_command`.

## Examples

`01_Basic`, `02_CustomModule` and `03_ModuleDependencies` in [`examples/`](examples/), in
increasing order of scope; the last one shows two modules where one requires the other.

## Dependencies

XeWeUtils, XeWeSerial, XeWeNvs, XeWeCli (and ArduinoJson through XeWeNvs).
For local development, clone the library repos next to this one:

```bash
cd ..   # the folder holding all xewe-labs repos
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc \
  --library xewe-library-utils --library xewe-library-serial --library xewe-library-nvs \
  --library xewe-library-cli --library xewe-library-os \
  xewe-library-os/examples/02_CustomModule
```

## Changes from the monolithic xewe-os

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
