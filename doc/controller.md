# ModuleController

`src/ModuleController/ModuleController.h` — owns the core services and every registered module.

```cpp
xewe::os::ModuleController os({
    .project_name    = "blink-device",
    .version         = "0.1.0",
    .build_timestamp = __DATE__ " " __TIME__,
});

BlinkModule blink(os, {.pin = 8});           // declared after the controller

void setup() { os.begin(); }
void loop()  { os.loop();  }
```

## ModuleControllerConfig

```cpp
struct ModuleControllerConfig {
    std::string            project_name    = "xewe-device";
    std::string            version         = "0.0.0";
    std::string            build_timestamp = {};
    std::string            url             = "https://github.com/xewe-labs/xewe-library-os";
    xewe::SerialPortConfig serial          = {};
    bool                   print_banner    = true;
};
```

| Field | Default | |
|---|---|---|
| `project_name` | `"xewe-device"` | first line of the boot header |
| `version` | `"0.0.0"` | printed as `Version <version>` |
| `build_timestamp` | empty | printed only when set; `__DATE__ " " __TIME__` is the usual value |
| `url` | this library's repo | printed under a separator; **set it to your own project or clear it** |
| `serial` | see [SerialPortConfig](https://github.com/xewe-labs/xewe-library-serial/blob/main/doc/config.md) | `115200` baud, 2048/1024 buffers, 1000 ms startup delay, echo on |
| `print_banner` | `true` | the ASCII XeWe banner at the top of the boot log |

## Core services

Public members, available to every module as `controller.<name>`:

| Member | Type | Library |
|---|---|---|
| `serial` | `xewe::SerialPort` | [XeWeSerial](https://github.com/xewe-labs/xewe-library-serial) |
| `nvs` | `xewe::Nvs` | [XeWeNvs](https://github.com/xewe-labs/xewe-library-nvs) |
| `xewe_cli` | `xewe::Cli` | [XeWeCli](https://github.com/xewe-labs/xewe-library-cli) |
| `system` | `xewe::os::System` | this library — see [system.md](system.md) |

`system` is a member, so it is always the first module registered and the first to begin.

## Constructor

```cpp
explicit ModuleController(ModuleControllerConfig config = {});

ModuleController(const ModuleController&)            = delete;
ModuleController& operator=(const ModuleController&) = delete;
```

Non-copyable. Modules hold a reference to it.

Member declaration order in the class is deliberate: the private `modules` vector and `config` are
declared **before** the public service members, so the vector already exists when `system`
constructs itself and registers.

## begin

```cpp
void begin();
```

Call once from `setup()`. In order:

1. `serial.begin(config.serial)` — which blocks for `startup_delay_ms`.
2. Installs an NVS error handler that prints to the console:
   `nvs.set_error_handler([this](std::string_view m) { serial.print(m); })`. XeWeNvs errors
   therefore appear on the serial port rather than in the ESP log.
3. Prints the banner, when `config.print_banner`.
4. Reads `init_setup_flag` from the **`root` NVS namespace**. It is unset on the very first boot
   of a device.
5. Calls `begin()` on every registered module, in registration order.
6. **On the first boot only:** prints `Initial Setup Complete`, writes `root/init_setup_flag`, and
   **restarts the device**.
7. Prints `System Setup Complete`.

**The first boot of a new device ends in a reboot.** Everything after `os.begin()` in `setup()` is
not reached on that boot. Anything with a one-time side effect outside NVS has to tolerate running
again.

Boot output order is: banner → the `System` project/version header → each module's
`<name> Setup` header → `System Setup Complete`.

## loop

```cpp
void loop();
```

Call every iteration. Runs `xewe_cli.loop()` first, then `loop()` on each module **that is
enabled** — a disabled module never has `loop()` called, which is why public functions of a
disableable module should guard with `if (is_disabled()) return;`.

## register_module

```cpp
bool register_module(Module& module);
```

Called from `Module`'s constructor; a sketch does not call it. Returns `false` and registers
nothing when a module with the same `id` already exists.

**The module constructor ignores that return value.** A duplicate id therefore leaves a module
unregistered — it never begins and never loops — while its CLI group still exists. Two modules
sharing an id is a silent failure; keep ids unique.

## get_module, get_modules, get_config

```cpp
Module*                       get_module (std::string_view id) const;
const std::vector<Module*>&   get_modules()                    const;
const ModuleControllerConfig& get_config ()                    const;
```

`get_module` returns `nullptr` when nothing matches; the match is exact and case-sensitive. It is
the way to reach a module you do not hold a reference to. Prefer a constructor reference plus
`add_requirement` for a real dependency — see [module.md](module.md).

`get_config` is how `System` reads the project name and version for the boot header.

## Declaration order

Modules register themselves from their constructors, and globals in one translation unit are
constructed top to bottom. So:

* the controller is declared **first**,
* a module is declared **after** every module it depends on,
* declaration order is begin order is loop order.

```cpp
xewe::os::ModuleController os({...});
Wifi         wifi(os);
WebInterface web(os, wifi);        // after wifi
```
