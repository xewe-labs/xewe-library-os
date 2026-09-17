# XeWeOS documentation

Complete reference for the XeWe OS core. The [README](../README.md) is the short version: what the
library is, how firmware is assembled, and how to write a module.

**[AGENTS.md](AGENTS.md) — read this first if you are a coding agent.** It applies to the whole
repository, not just this folder.

| Page | Covers |
|---|---|
| [controller.md](controller.md) | `ModuleController`, `ModuleControllerConfig` and its defaults, the boot sequence, `register_module`, `get_module`, `get_modules`, `get_config`, declaration order |
| [module.md](module.md) | `Module` — the constructor flags, the eight-step lifecycle, `add_requirement`, `enable`/`disable`/`reset`, the info getters, `register_command`, `run_with_dots`, the generic `$<id>` commands, the NVS keys |
| [system.md](system.md) | `System` — every `$system` command, the boot header, the factory reset, `get_device_name`, `restart` |

```cpp
#include <XeWeOS.h>
```

## Things that surprise people

* **The first boot of a new device ends in an automatic reboot.** Nothing after `os.begin()` runs
  on that boot.
* **`disable()` implies `reset()`** — it wipes the module's NVS namespace — **and cascades to
  every dependent module.** There is no confirmation unless `verbose` is true.
* `$<id> reset` reboots and wipes without asking. `$system reset` is a full factory reset and
  *does* ask — and refuses to run at all when called from code.
* `System::begin_routines_required()` calls `esp_log_level_set("*", ESP_LOG_NONE)`, silencing
  ESP-IDF logging for the whole firmware.
* **A module `id` is both the CLI group and the NVS namespace,** so it is capped at 15 characters.
* `enable`, `disable` and `reset` all default to `do_restart = true`.
* `status()` reads the enabled flag from NVS, not from the in-RAM flag.
* A disabled module never gets `loop()`, but stays registered and callable — guard public
  functions with `if (is_disabled()) return;`.
* **Never name an object `cli`.** The Arduino core defines it as a macro; the core uses
  `xewe_cli`.

## Core services

| Member | Type | Library |
|---|---|---|
| `os.serial` | `xewe::SerialPort` | [XeWeSerial](https://github.com/xewe-labs/xewe-library-serial) |
| `os.nvs` | `xewe::Nvs` | [XeWeNvs](https://github.com/xewe-labs/xewe-library-nvs) |
| `os.xewe_cli` | `xewe::Cli` | [XeWeCli](https://github.com/xewe-labs/xewe-library-cli) |
| `os.system` | `xewe::os::System` | this library |

Also depends on [XeWeUtils](https://github.com/xewe-labs/xewe-library-utils), and on ArduinoJson
through XeWeNvs.

## Examples

Three sketches, in increasing order of scope:

| | | |
|---|---|---|
| low | [`01_Basic`](../examples/01_Basic) | the minimum sketch: a controller, `begin()`, `loop()` |
| mid | [`02_CustomModule`](../examples/02_CustomModule) | one module with config, a CLI command, init setup and a non-blocking loop |
| high | [`03_ModuleDependencies`](../examples/03_ModuleDependencies) | two modules where one requires the other: the cascade, a `status()` override, and a `get_module` lookup |

Also [`extras/ModuleTemplate`](../extras/ModuleTemplate) — a copy-paste skeleton with every hook
commented.

## Local development

Clone the library repos side by side and pass them to the compiler:

```bash
cd ..   # the folder holding all xewe-labs repos
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc \
  --library xewe-library-utils --library xewe-library-serial --library xewe-library-nvs \
  --library xewe-library-cli --library xewe-library-os \
  xewe-library-os/examples/02_CustomModule
```
